// Remote executable launch proof of concept

#define _GNU_SOURCE

#define TEST_RDMA_SYNC 0

#include <fcntl.h>
#include <libgen.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dccs_parameters.h"
#include "dccs_utils.h"
#include "dccs_rdma.h"

#define COPY_MMAP_EXEC 0

uint64_t clock_rate = 0;    // Clock ticks per second

int run(struct dccs_parameters params) {
    struct rdma_cm_id *listen_id = NULL, *id;
    int rv = 0;

    Role role = params.server == NULL ? ROLE_SERVER : ROLE_CLIENT;
    if (role == ROLE_CLIENT)
        log_info("Running in client mode ...\n");
    else
        log_info("Running in server mode ...\n");

    if (role == ROLE_CLIENT) {
        if ((rv = dccs_connect(&id, params.server, params.port, params.tos)) != 0)
            goto end;
    } else {    // role == ROLE_SERVER
        if ((rv = dccs_listen(&listen_id, &id, params.port)) != 0)
            goto end;
    }

    size_t exec_size;
    if (role == ROLE_CLIENT) {
        struct stat sb;
        exec_size = stat(params.exec, &sb) == 0 ? (size_t)sb.st_size : 0;

        if (send_message(id, &exec_size, sizeof exec_size) < 0) {
            log_error("Failed to send exec size.\n");
            goto out_disconnect;
        }
    } else {    // role == ROLE_SERVER
        if (recv_message(id, &exec_size, sizeof exec_size) < 0) {
            log_error("Failed to recv exec size.\n");
            goto out_disconnect;
        }
    }

log_debug("exec_size = %zu.\n", exec_size);
    log_debug("Allocating buffer for executable ...\n");
    int fd;
    void *buf, *sendbuf;
    char tmpfile[256];
    char *filename = NULL;
    unsigned char local_digest[SHA_DIGEST_LENGTH];
    if (role == ROLE_CLIENT) {
        if ((fd = open(params.exec, O_RDWR)) < 0) {
            log_perror("open");
            goto out_disconnect;
        }

        buf = mmap(NULL, exec_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buf == MAP_FAILED) {
            log_error("Failed to mmap file \"%s\"(%d).\n", params.exec, fd);
            goto out_disconnect;
        }
#if COPY_MMAP_EXEC
        void *buf2 = malloc(exec_size);
        memcpy(buf2, buf, exec_size);
        sendbuf = buf2;
#else
        sendbuf = buf;
#endif
        if (send_message(id, sendbuf, exec_size) < 0) {
            log_error("Failed to send executable.\n");
            goto out_disconnect;
        }

        sha1sum(sendbuf, exec_size, local_digest);
        if (send_message(id, local_digest, SHA_DIGEST_LENGTH) < 0) {
            log_error("Failed to send digest.\n");
            goto out_disconnect;
        }
#if COPY_MMAP_EXEC
        free(buf2);
#endif

        char *digest_hex = bin_to_hex_string(local_digest, SHA_DIGEST_LENGTH);
        log_info("Sent executable \"%s\", size = %zu bytes, shasum = %s.\n",
                    params.exec, exec_size, digest_hex);
        free(digest_hex);
    } else {    // role == ROLE_SERVER
        filename = basename(params.exec);
        if (sprintf(tmpfile, "%s/%s", TMPFS_PATH, filename) < 0) {
            log_error("Failed to create new file name under tmpfs.\n");
            goto out_disconnect;
        }

        if ((fd = open(tmpfile, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
            log_perror("creat");
            goto out_disconnect;
        }

        if (lseek(fd, (off_t)exec_size, SEEK_SET) < 0) {
            log_perror("lseek");
            goto out_disconnect;
        }

        if (write(fd, "", 1) < 0) {
            close(fd);
            log_perror("write");
            goto out_disconnect;
        }

        buf = mmap(NULL, exec_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (buf == MAP_FAILED) {
            log_perror("mmap");
            log_error("Failed to mmap file \"%s\"(%d).\n", tmpfile, fd);
            goto out_disconnect;
        }

        if (recv_message(id, buf, exec_size) < 0) {
            log_error("Failed to recv executable.\n");
            goto out_disconnect;
        }

        unsigned char source_digest[SHA_DIGEST_LENGTH];
        sha1sum(buf, exec_size, local_digest);
        if (recv_message(id, source_digest, SHA_DIGEST_LENGTH) < 0) {
            log_error("Failed to recv digest.\n");
            goto out_disconnect;
        }

        if (strncmp((char *)source_digest, (char *)local_digest, SHA_DIGEST_LENGTH) != 0) {
            log_error("Bad SHA sum, aborting ...\n");
            goto out_disconnect;
        }

        char *digest_hex = bin_to_hex_string(local_digest, SHA_DIGEST_LENGTH);
        log_info("Received executable \"%s\", size = %zu bytes, shasum = %s.\n",
                    tmpfile, exec_size, digest_hex);
        free(digest_hex);
    }

    if (munmap(buf, exec_size) != 0)
        log_perror("munmap");
    if (close(fd) != 0)
        log_perror("close");

    if (role == ROLE_CLIENT) {

    } else {    // role == ROLE_SERVER
        log_debug("Launching executable \"%s\"...\n", tmpfile);

        pid_t pid;
        int status;

        if ((pid = fork()) < 0) {
            log_perror("fork");
            goto out_disconnect;
        } else if (pid == 0) {
            char* exec_argv[2] = { filename, NULL };
            if (execv(tmpfile, exec_argv) < 0) {
                log_perror("execv");
                goto out_disconnect;
            }
        } else {
            while (wait(&status) != pid);
        }
    }

    if (role == ROLE_CLIENT) {

    } else {    // role == ROLE_SERVER

    }

    // Synchronize end of a round
    if (role == ROLE_CLIENT) {
        log_debug("Sending terminating message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = SYNC_END_MESSAGE;
        if ((rv = send_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to send terminating message.\n");
            goto out_disconnect;
        }
    } else {    // role == ROLE_SERVER
        log_debug("Waiting for end message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = {0};
        if ((rv = recv_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to recv terminating message.\n");
            goto out_disconnect;
        }
    }

out_disconnect:
    log_debug("Disconnecting\n");
    if (role == ROLE_CLIENT)
        dccs_client_disconnect(id);
    else    // role == ROLE_SERVER
        dccs_server_disconnect(id, listen_id);
end:
    return rv;
}

int main(int argc, char *argv[]) {
    struct dccs_parameters params;

    parse_args(argc, argv, &params);
    print_parameters(&params);
    dccs_init();

    return run(params);
}

