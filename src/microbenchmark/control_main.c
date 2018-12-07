// Control signal testing program
// Server is the control host, client(s) is/are the end hosts to be synchronized.
// Both sides use RDMA writes and busy looping to check receipt of data.


#define _GNU_SOURCE

#define MAGIC 0xd1cefa11
#define SIGNAL_INTERVAL 1000
#define SLOT_NS 100000

#include <assert.h>
#include <stdio.h>

#include "dccs_parameters.h"
#include "dccs_utils.h"
#include "dccs_rdma.h"

uint64_t clock_rate = 0;    // Clock ticks per second

inline static void wait_until(uint64_t target) {
    while (get_cycles() < target);
}

int run(struct dccs_parameters params) {
    struct rdma_cm_id *listen_id = NULL, *id;
    struct ibv_wc wc;
    struct dccs_request *requests_out, *requests_in;
    uint64_t *start = NULL, *end = NULL;
    int rv = 0;

    Role role = params.server == NULL ? ROLE_SERVER : ROLE_CLIENT;
    if (role == ROLE_CLIENT)
        log_info("Running in client mode ...\n");
    else
        log_info("Running in server mode ...\n");

    assert(params.count == 1);
    assert(params.length == 6);
    assert(params.verb == Write);

    if (role == ROLE_CLIENT) {
        if ((rv = dccs_connect(&id, params.server, params.port, params.tos)) != 0)
            goto end;
    } else {    // role == ROLE_SERVER
        if ((rv = dccs_listen(&listen_id, &id, params.port)) != 0)
            goto end;
    }

    log_debug("Allocating buffer ...\n");
    requests_in = calloc(params.count, sizeof(struct dccs_request));
    if ((rv = allocate_buffer(id, requests_in, params)) != 0) {
        log_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }
    requests_out = calloc(params.count, sizeof(struct dccs_request));
    if ((rv = allocate_buffer(id, requests_out, params)) != 0) {
        log_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

    uint32_t magic = htonl(MAGIC);
    uint16_t ts_index = htons(params.index);
    for (size_t n = 0; n < params.count; n++) {
        struct dccs_request *request = requests_out + n;
        if (role == ROLE_CLIENT) {
            memcpy(request->buf, &magic, sizeof magic);
            memcpy((char *)request->buf + sizeof magic, &ts_index, sizeof ts_index);
        } else {    // Same for now
            memcpy(request->buf, &magic, sizeof magic);
            memcpy((char *)request->buf + sizeof magic, &ts_index, sizeof ts_index);
        }
    }

    if (role == ROLE_SERVER) {
        log_debug("Getting remote MR info ...\n");
        rv = get_remote_mr_info(id, requests_out, params.count);
        if (rv < 0) {
            log_debug("rv = %d.\n", rv);
            log_error("Failed to get remote MR info.\n");
            goto out_deallocate_buffer;
        }

        log_debug("Sending local MR info ...\n");
        rv = send_local_mr_info(id, requests_in, params.count);
        if (rv < 0) {
            log_error("Failed to send remote MR info.\n");
            goto out_deallocate_buffer;
        }
    } else {    // role == ROLE_CLIENT
        log_debug("Sending local MR info ...\n");
        rv = send_local_mr_info(id, requests_in, params.count);
        if (rv < 0) {
            log_error("Failed to send remote MR info.\n");
            goto out_deallocate_buffer;
        }

        log_debug("Getting remote MR info ...\n");
        rv = get_remote_mr_info(id, requests_out, params.count);
        if (rv < 0) {
            log_debug("rv = %d.\n", rv);
            log_error("Failed to get remote MR info.\n");
            goto out_deallocate_buffer;
        }
    }

    for (size_t n = 0; n < params.count; n++) {
        struct dccs_request *request = requests_out + n;
        log_debug("out: remote addr = %#10x, in: addr = %#10x\n",
                request->remote_addr, request->buf);
    }

    log_debug("Sending RDMA writes ...\n");
    if (role == ROLE_SERVER) {
        start = calloc(params.repeat * params.count, sizeof(uint64_t));
        if (start == NULL) {
            perror("calloc");
            goto out_deallocate_buffer;
        }
        end = calloc(params.repeat * params.count, sizeof(uint64_t));
        if (end == NULL) {
            perror("calloc");
            goto out_deallocate_buffer;
        }
    }

    //uint32_t recvd = 0;
    int flag;
    uint64_t target = get_cycles();
    size_t requests_sent = 0;
    for (size_t n = 0; n < params.repeat; n++) {
        if (n % (params.repeat / 100) == 0)
        //if (n % 100 == 0)
            log_debug("n = %zu.\n", n);

        // Note: this is not working!
        //target = (get_cycles() - target) / SLOT_NS * SLOT_NS + SLOT_NS;
        target += SLOT_NS;
        wait_until(target);

        // Note: we need to signal occasionally;
        //  otherwise, with only unsignaled requests, WQ will be full,
        //  as they won't generate CQE.
        bool signal = (requests_sent % SIGNAL_INTERVAL == 0);
        flag = signal ? IBV_SEND_SIGNALED : 0;

        for (size_t i = 0; i < params.count; i++) {
            struct dccs_request *request_in = requests_in + i;
            struct dccs_request *request_out = requests_out + i;
            if (role == ROLE_SERVER) {
                start[n * params.count + i] = get_cycles();
                rv = dccs_rdma_write_with_flags(id,
                        request_out->buf, request_out->length, request_out->mr,
                        request_out->remote_addr, request_out->remote_rkey, flag);
                requests_sent++;
                if (rv != 0) {
                    log_error("Failed to send write.\n");
                    break;
                }
                /*while (magic != recvd) {
                    sleep(1);
                    recvd = *((uint32_t *)request_in->buf);
                    log_debug("recvd = %#010x\n", recvd);
                }*/
                while (magic != *((volatile uint32_t *)request_in->buf));
                end[n * params.count + i] = get_cycles();
                memset(request_in->buf, 0, request_in->length);
            } else {    // role == ROLE_CLIENT
                /*while (magic != recvd) {
                    sleep(1);
                    recvd = *((uint32_t *)request_in->buf);
                    log_debug("recvd = %#010x\n", recvd);
                }*/
                while (magic != *((volatile uint32_t *)request_in->buf));
                rv = dccs_rdma_write_with_flags(id,
                        request_out->buf, request_out->length, request_out->mr,
                        request_out->remote_addr, request_out->remote_rkey, flag);
                requests_sent++;
                if (rv != 0) {
                    log_error("Failed to send write.\n");
                    break;
                }

                memset(request_in->buf, 0, request_in->length);
            }

            if (signal) {
                while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
                if (rv < 0) {
                    log_error("Failed to send comp message.\n");
                    break;
                }
            }
        }
    }

    // Synchronize end of a round
    if (role == ROLE_SERVER) {
        log_debug("Sending terminating message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = SYNC_END_MESSAGE;
        if ((rv = send_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to send terminating message.\n");
            goto out_deallocate_buffer;
        }
    } else {    // role == ROLE_CLIENT
        log_debug("Waiting for end message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = {0};
        if ((rv = recv_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to recv terminating message.\n");
            goto out_deallocate_buffer;
        }
    }

    if (role == ROLE_SERVER) {
        print_latency_report_raw(start, end, params.repeat * params.count, start[0], params.verbose, params.count, params.length);
    }

    // Print stats
    if (role == ROLE_SERVER) {
        log_info("Server sent:\n");
        print_sha1sum(requests_out, params.count);
        log_info("Server received:\n");
        print_sha1sum(requests_in, params.count);
    } else {
        log_info("Client received:\n");
        print_sha1sum(requests_in, params.count);
        log_info("Client sent:\n");
        print_sha1sum(requests_out, params.count);
    }

out_deallocate_buffer:
    if (role == ROLE_SERVER) {
        if (start != NULL)
            free(start);
        if (end != NULL)
            free(end);
    }

    log_debug("de-allocating buffer\n");
    deallocate_buffer(requests_in, params);
    deallocate_buffer(requests_out, params);
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

