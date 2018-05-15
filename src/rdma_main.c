// RDMA tool

#define _GNU_SOURCE

#include <stdio.h>

#include "dccs_config.h"
#include "dccs_utils.h"
#include "dccs_parameters.h"
#include "dccs_rdma.h"
#include "rdma_client.h"
#include "rdma_server.h"

uint64_t clock_rate = 0;    // Clock ticks per second

int main(int argc, char *argv[]) {
    struct dccs_parameters params;
    int rv;

    parse_args(argc, argv, &params);
    print_parameters(&params);
    dccs_init();

    if (params.server == NULL) {
        log_info("Running in server mode ...\n");
        rv = run_server(params);
    } else {
        log_info("Running in client mode ...\n");
        rv = run_client(params);
    }

    return rv;
}

