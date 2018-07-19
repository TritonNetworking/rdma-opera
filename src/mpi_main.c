// MPI tool

#define _GNU_SOURCE

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "dccs_utils.h"

#define MPI_USE_ASYNC_VERB 1    // Whether to use asynchronous send/recv
#define MPI_USE_WAIT 0          // Whether to use wait (or test)

uint64_t clock_rate = 0;    // Clock ticks per second

void wait_for_gdb(int rank) {
    if (rank != 0)
        return;

    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
        sleep(5);
}

int verify_checksum(const void *buf, size_t buffer_size, int rank, int size) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    sha1sum(buf, buffer_size, digest);
    char *digest_hex = bin_to_hex_string(digest, SHA_DIGEST_LENGTH);
    if (rank == 0) {
        //log_info("Verifying checksum ...\n");
        MPI_Status status;
        for (int src = 1; src < size; src++) {
            unsigned char remote_digest[SHA_DIGEST_LENGTH];
            MPI_Recv(remote_digest, SHA_DIGEST_LENGTH, MPI_CHAR, src, 0, MPI_COMM_WORLD, &status);
            if (strncmp((const void *)digest, (const void *)remote_digest, SHA_DIGEST_LENGTH) != 0) {
                log_error("Incorrect SHA sum from rank %d.\n", src);
                return -1;
            }
        }

        //log_info("Checksum verified.\n");
    } else {
        MPI_Send(digest, SHA_DIGEST_LENGTH, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }

    free(digest_hex);
    return 0;
}

#define HOST_ALL -1
#define HOST_NOSELF - 2

int send_messages(int size, int rank, const void *buf, struct dccs_parameters params, int to, size_t *bytes_sent) {
    if (to != HOST_ALL && to != HOST_NOSELF && (to < 0 || to >= size)) {
        log_error("Invalid send destination %d.\n", to);
        return -1;
    }

#if MPI_USE_ASYNC_VERB
    size_t request_count = 1;
    if (to == HOST_ALL)
        request_count = (size_t)size;
    else if (to == HOST_NOSELF)
        request_count = (size_t)(size - 1);
    MPI_Request requests[request_count * params.count];
    int request_sent = 0;
#endif

    int length = (int)params.length;
    void *sendbuf;
    for (size_t n = 0; n < params.count; n++) {
        sendbuf = (void *)((uint8_t *)buf + n * params.length);
        for (int dest = 0; dest < size; dest++) {
            if (to == HOST_NOSELF && dest == rank)
                continue;
            else if (to != HOST_ALL && to != HOST_NOSELF && dest != to)
                continue;

#if MPI_USE_ASYNC_VERB
            MPI_Isend(sendbuf, length, MPI_BYTE, dest, 0, MPI_COMM_WORLD, requests + request_sent);
#if MPI_FIRE_AND_FORGET
            MPI_Request_free(requests + request_sent);
#endif
            request_sent++;
#else
            MPI_Send(sendbuf, length, MPI_BYTE, dest, 0, MPI_COMM_WORLD);
#endif
            *bytes_sent += params.length;
        }
    }

#if MPI_USE_ASYNC_VERB && !MPI_FIRE_AND_FORGET
#if MPI_USE_WAIT
    MPI_Waitall(request_count * params.count, requests, MPI_STATUSES_IGNORE);
#else
    int done = 0;
    while (!done) {
        MPI_Testall(request_count * params.count, requests, &done, MPI_STATUSES_IGNORE);
    }
#endif
#endif

    return 0;
}

int recv_messages(int size, int rank, const void *buf, struct dccs_parameters params, int from, size_t *bytes_recvd) {
    if (from != HOST_ALL && from != HOST_NOSELF && (from < 0 || from >= size)) {
        log_error("Invalid receive source %d.\n", from);
        return -1;
    }

#if MPI_USE_ASYNC_VERB
    size_t request_count = 1;
    if (from == HOST_ALL)
        request_count = (size_t)size;
    else if (from == HOST_NOSELF)
        request_count = (size_t)(size - 1);
    MPI_Request requests[request_count * params.count];
    int request_sent = 0;
#endif

    int length = (int)params.length;
    void *recvbuf;
    for (size_t n = 0; n < params.count; n++) {
        recvbuf = (void *)((uint8_t *)buf + n * params.length);
        for (int src = 0; src < size; src++) {
            if (from == HOST_NOSELF && src == rank)
                continue;
            else if (from != HOST_ALL && from != HOST_NOSELF && src != from)
                continue;

#if MPI_USE_ASYNC_VERB
            MPI_Irecv(recvbuf, length, MPI_BYTE, src, 0, MPI_COMM_WORLD, requests + request_sent);
            request_sent++;
#else
            MPI_Recv(recvbuf, length, MPI_BYTE, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#endif
            *bytes_recvd += params.length;
        }
    }

#if MPI_USE_ASYNC_VERB
#if MPI_USE_WAIT
    MPI_Waitall(request_count * params.count, requests, MPI_STATUSES_IGNORE);
#else
    int done = 0;
    while (!done) {
        int total_count = (int)(request_count * params.count);
        MPI_Testall(total_count, requests, &done, MPI_STATUSES_IGNORE);
    }
#endif
#endif

    return 0;
}

int run(int size, int rank, struct dccs_parameters params) {
    int rv = 0;
    void *buf;
    uint64_t start, end;
    bool should_send, should_recv;
    int send_target, recv_source;

    size_t bytes_sent, bytes_recvd;
    size_t buffer_size = params.length * params.count;

    buf = malloc_random(buffer_size);

    switch (params.direction) {
        case DIR_OUT:
            should_send = (rank == 0);
            should_recv = !should_send;
            send_target = HOST_NOSELF;
            recv_source = 0;
            break;
        case DIR_IN:
            should_send = (rank != 0);
            should_recv = !should_send;
            send_target = 0;
            recv_source = HOST_NOSELF;
            break;
        case DIR_BOTH:
            should_send = true;
            should_recv = true;
            send_target = HOST_NOSELF;
            recv_source = HOST_NOSELF;
            break;
        default:
            log_error("Unknown direction: %d.\n", params.direction);
            exit(EXIT_FAILURE);
            break;
    }

    //MPI_Barrier(MPI_COMM_WORLD);
    //start = get_cycles();

    for (size_t r = 0; r < params.repeat; r++) {
        MPI_Barrier(MPI_COMM_WORLD);

        //buf = malloc_random(buffer_size);
        bytes_sent = bytes_recvd = 0;

        if (should_send) {
            //start = get_cycles();
            send_messages(size, rank, buf, params, send_target, &bytes_sent);
            //end = get_cycles();
        }

        if (should_recv) {
            start = get_cycles();
            recv_messages(size, rank, buf, params, recv_source, &bytes_recvd);
            end = get_cycles();
        }

        if (should_recv) {
            double elapsed = (double)(end - start) / (double)clock_rate;
            double elapsed_usec = elapsed * 1e6;
            double throughput_gbits = (double)bytes_recvd * 8 / elapsed / (1024 * 1024 * 1024);
            log_info("round = %zu, rank = %d, bytes recv'd = %zu, elapsed = %.3fµsec, throughput = %.3f gbits.\n", r, rank, bytes_recvd, elapsed_usec, throughput_gbits);
        }

        //verify_checksum(buf, buffer_size, rank, size);
        //free(buf);
    }

    verify_checksum(buf, buffer_size, rank, size);
    free(buf);

/*
    end = get_cycles();
    double elapsed = (double)(end - start) / clock_rate;
    double elapsed_usec = elapsed * 1e6;
    double throughput_gbits = bytes_sent * 8 / elapsed / (1024 * 1024 * 1024);
    log_info("rank = %d, bytes transferred = %zu, elapsed = %.3fµsec, throughput = %.3f gbits.\n", rank, bytes_sent, elapsed_usec, throughput_gbits);
*/

    return rv;
}

int main(int argc, char *argv[]) {
    int size, rank, rv;
    struct dccs_parameters params;

    parse_args(argc, argv, &params);
    print_parameters(&params);
    dccs_init();

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //wait_for_gdb(rank);

    rv = run(size, rank, params);

    MPI_Finalize();

    return rv;
}

