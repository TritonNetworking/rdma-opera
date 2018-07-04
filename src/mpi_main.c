#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#include "dccs_util.h"

#define MESSAGE_SIZE 65536
#define MESSAGE_COUNT 1000

int run(int size, int rank, struct dccs_parameters params) {
    int rv = 0;
    void *buf, *sendbuf, *recvbuf;
    uint64_t start, end;

    size_t bytes_sent = 0;
    size_t buffer_size = params.length * params.count;

    if (rank == 0) {
        MPI_Request requests[(size - 1) * params.count];
        MPI_Status statuses[(size - 1) * params.count];
        buf = malloc_random(buffer_size);

        MPI_Barrier(MPI_COMM_WORLD);

        start = get_cycles();
        for (int n = 0; n < params.count; n++) {
            sendbuf = buf + n * params.length;
            for (int dest = 1; dest < size; dest++) {
                MPI_Isend(sendbuf, params.length, MPI_Byte, dest, 0, MPI_COMM_WORLD, requests + n * (size - 1) + dest);
                bytes_sent += params.length;
            }
        }

        MPI_Waitall(params.count, requests, &statuses);
        end = get_cycles();
    } else {
        MPI_Status status;
        buf = malloc(buffer_size);

        MPI_Barrier(MPI_COMM_WORLD);
        start = get_cycles();
        for (int n = 0; n < params.count; n++) {
            recvbuf = buf + n * params.length;
            MPI_Recv(recvbuf, params.length, MPI_Byte, source, 0, MPI_COMM_WORLD, &status);
            bytes_sent += params.length;
        }

        end = get_cycles();
    }

    free(buf);

    double elapsed = (double)(end - start) / clock_rate;
    double elapsed_µsec = elapsed * 1e6;
    double throughput_gbits = bytes_sent * 8 / elapsed / 1e9;
    log_info("rank = %d, bytes sent = %zu, elapsed = %.3fµsec, throughput = %.3f gbits.\n", rank, bytes_sent, elapsed_µsec, throughput_gbits);

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

