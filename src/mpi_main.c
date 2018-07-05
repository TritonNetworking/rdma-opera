// MPI tool

#define _GNU_SOURCE

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dccs_utils.h"

#define REPEAT 10

uint64_t clock_rate = 0;    // Clock ticks per second

int verify_checksum(const void *buf, size_t buffer_size, int rank, int size) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    sha1sum(buf, buffer_size, digest);
    char *digest_hex = bin_to_hex_string(digest, SHA_DIGEST_LENGTH);
    if (rank == 0) {
        //log_info("Verifying checksum ...\n");
        MPI_Status status;
        for (size_t src = 1; src < size; src++) {
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

int run(int size, int rank, struct dccs_parameters params) {
    int rv = 0;
    void *buf, *sendbuf, *recvbuf;
    uint64_t start, end;

    size_t bytes_sent = 0;
    size_t buffer_size = params.length * params.count;

    buf = malloc_random(buffer_size);

    //MPI_Barrier(MPI_COMM_WORLD);
    //start = get_cycles();

    for (size_t r = 0; r < REPEAT; r++) {
        //buf = malloc_random(buffer_size);

        int done = 0;
        bytes_sent = 0;
        if (rank == 0) {
            MPI_Request requests[(size - 1) * params.count];

            MPI_Barrier(MPI_COMM_WORLD);
            start = get_cycles();
            for (size_t n = 0; n < params.count; n++) {
                sendbuf = (void *)((uint8_t *)buf + n * params.length);
                for (size_t dest = 1; dest < size; dest++) {
                    MPI_Isend(sendbuf, params.length, MPI_BYTE, dest, 0, MPI_COMM_WORLD, requests + n * (size - 1) + dest - 1);
                    //MPI_Send(sendbuf, params.length, MPI_BYTE, dest, 0, MPI_COMM_WORLD);
                    bytes_sent += params.length;
                }
            }

            //MPI_Waitall((size - 1) * params.count, requests, MPI_STATUSES_IGNORE);
            while (!done) {
                MPI_Testall((size - 1) * params.count, requests, &done, MPI_STATUSES_IGNORE);
            }

            end = get_cycles();
        } else {
            int source = 0;
            MPI_Request requests[params.count];

            MPI_Barrier(MPI_COMM_WORLD);
            start = get_cycles();
            for (size_t n = 0; n < params.count; n++) {
                recvbuf= (void *)((uint8_t *)buf + n * params.length);
                MPI_Irecv(recvbuf, params.length, MPI_BYTE, source, 0, MPI_COMM_WORLD, requests + n);
                //MPI_Recv(recvbuf, params.length, MPI_BYTE, source, 0, MPI_COMM_WORLD, &status);
                bytes_sent += params.length;
            }

            //MPI_Waitall(params.count, requests, MPI_STATUSES_IGNORE);
            while (!done) {
                MPI_Testall(params.count, requests, &done, MPI_STATUSES_IGNORE);
            }

            end = get_cycles();
        }

        double elapsed = (double)(end - start) / clock_rate;
        double elapsed_usec = elapsed * 1e6;
        double throughput_gbits = bytes_sent * 8 / elapsed / (1024 * 1024 * 1024);
        if (rank == 1)
            log_info("rank = %d, bytes transferred = %zu, elapsed = %.3fµsec, throughput = %.3f gbits.\n", rank, bytes_sent, elapsed_usec, throughput_gbits);

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

    rv = run(size, rank, params);

    MPI_Finalize();

    return rv;
}

