// MPI tool

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <mpi.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

#define TEST_SYNC_PACKET 0
#define SYNC_PACKET_HEADER "sync"
#define USE_MPIWTIME 1
#define MPI_USE_ASYNC_VERB 1    // Whether to use asynchronous send/recv
#define MPI_USE_WAIT 0          // Whether to use wait (or test)

#include "dccs_utils.h"

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
        for (int src = 1; src == 1; src++) {
            unsigned char remote_digest[SHA_DIGEST_LENGTH];
            MPI_Recv(remote_digest, SHA_DIGEST_LENGTH, MPI_CHAR, src, 0, MPI_COMM_WORLD, &status);
            if (strncmp((const char *)digest, (const char *)remote_digest, SHA_DIGEST_LENGTH) != 0) {
                log_error("Incorrect SHA sum from rank %d.\n", src);
                return -1;
            }
        }

        //log_info("Checksum verified.\n");
    } else if (rank == 1) {
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

#if TEST_SYNC_PACKET
    if (rank == 0)
        log_debug("slot is %hhu.\n", params.slot);

    uint8_t slot = params.slot;
    params.count = 1;
    params.length = strlen(SYNC_PACKET_HEADER) + sizeof slot;

#endif

    size_t buffer_size = params.length * params.count;
    buf = malloc_random(buffer_size);

#if TEST_SYNC_PACKET
    if (rank == 0) {
        strcpy((char *)buf, SYNC_PACKET_HEADER);
        memcpy((char *)buf + strlen(SYNC_PACKET_HEADER), &slot, sizeof slot);
        char *s = bin_to_hex_string(buf, params.length);
        printf("Packet: %s\n", s);
        free(s);
    }
#endif

    switch (params.direction) {
        case DIR_OUT:
            should_send = (rank == 0);
            should_recv = (rank == 1);
            send_target = 1;
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

    fprintf(stderr, "[%d] send = %d, recv = %d.\n", rank, should_send, should_recv);

    for (size_t r = 0; r < params.repeat; r++) {
        //MPI_Barrier(MPI_COMM_WORLD);

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

    fprintf(stderr, "[%d] out of loop.\n", rank);

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

#if TEST_SYNC_PACKET

#define TAG_SYNC_BARRIER 1000

int Sync_Barrier(int size, int rank, uint8_t *slot) {
    const char *header = SYNC_PACKET_HEADER;
    const int header_len = strlen(header);
    size_t len = header_len + sizeof *slot;
    char buf[len];
    int rv;

    int tag = TAG_SYNC_BARRIER;
    if (rank == 0) {
        memcpy(buf, header, header_len);
        memcpy(buf + header_len, slot, sizeof *slot);
        MPI_Request *requests = (MPI_Request *)malloc((size - 1) * sizeof(MPI_Request));
        for (int dst = 1; dst < size; dst++) {
            rv = MPI_Isend(buf, len, MPI_CHAR, dst, tag, MPI_COMM_WORLD, requests + dst - 1);
            if (rv != 0) {
                log_perror("MPI_Isend");
            }
        }

        rv = MPI_Waitall(size - 1, requests, MPI_STATUSES_IGNORE);
        free(requests);

        if (rv != 0) {
            log_perror("MPI_Waitall");
            return -1;
        }

        return 0;
    } else {
        const int src = 0;
        rv = MPI_Recv(buf, len, MPI_CHAR, src, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (rv != 0) {
            log_error("Failed to receive sync barrier.\n");
            return -1;
        }

        if (strncmp(header, buf, header_len) != 0) {
            log_error("Incorrect sync header.\n");
            return -1;
        } else {
            memcpy(slot, buf + header_len, sizeof *slot);
            return 0;
        }
    }
}

#define INTERVAL 100000
#define SLOTS 255

int run1(int size, int rank, struct dccs_parameters params) {
    int rv;
    uint8_t slot;

    printf("[%d] Before barrier.\n", rank);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("[%d] After barrier.\n", rank);

    if (rank == 0) {
        // Sync node
        slot = 0;

        long long next = 0;
        auto start = std::chrono::high_resolution_clock::now();
        while (slot < SLOTS) {
            auto elapsed = std::chrono::high_resolution_clock::now() - start;
            long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
            if (ns >= next) {
                slot++;
                next += INTERVAL;
                //printf("[%d] slot = %d, elapsed = %lluus.\n", rank, slot, ns/1000);
                Sync_Barrier(size, rank, &slot);
            }
        }

        return rv;
    } else {
        // Non-sync node
        auto last = std::chrono::high_resolution_clock::now(), curr = last;
        while (slot < SLOTS) {
            Sync_Barrier(size, rank, &slot);
            last = curr;
            curr = std::chrono::high_resolution_clock::now();
            auto elapsed = curr - last;
            long long us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
            printf("[%d] slot = %d, elapsed = %lluus.\n", rank, slot, us);
        }

        return rv;
    }
}

#endif

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

#if TEST_SYNC_PACKET
    rv = run(size, rank, params);
#else
    rv = run(size, rank, params);
#endif

    fprintf(stderr, "[%d] before finalize.\n", rank);
    MPI_Finalize();

    return rv;
}

