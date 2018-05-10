// RDMA Client

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "dccs_config.h"
#include "dccs_utils.h"
#include "dccs_parameters.h"
#include "dccs_rdma.h"

int run_client(struct dccs_parameters params) {
    struct rdma_cm_id *id;
    struct rdma_addrinfo *res;
    struct dccs_request *requests;
    int rv = 0;

    if ((rv = dccs_connect(&id, &res, params.server, params.port)) != 0)
        goto end;

log_debug("Allocating buffer ...\n");
    size_t requests_size = params.count * sizeof(struct dccs_request);
    requests = malloc(requests_size);
    memset(requests, 0, requests_size);
    if ((rv = allocate_buffer(id, requests, params.length, params.count, Read)) != 0) {
        log_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

log_debug("Getting remote MR info ...\n");
    if ((rv = get_remote_mr_info(id, requests, params.count)) < 0) {
        log_debug("rv = %d.\n", rv);
        log_error("Failed to get remote MR info.\n");
        goto out_deallocate_buffer;
    }

/*
log_debug("Sending RDMA requests ...\n");
    if ((rv = send_requests(id, requests, params.count)) < 0) {
        log_error("Failed to send all requests.\n");
        goto out_deallocate_buffer;
    }

log_debug("Waiting for RDMA requests completion.\n");
    if ((rv = wait_requests(id, requests, params.count)) < 0) {
        log_error("Failed to send comp all requests.\n");
        goto out_deallocate_buffer;
    }
*/

    for (size_t n = 0; n < DEFAULT_REPEAT_COUNT; n++) {
        log_debug("Round %zu.\n", n + 1);
        log_debug("Sending and waiting for RDMA requests ...\n");
        if ((rv = send_and_wait_requests(id, requests, &params)) < 0) {
            log_error("Failed to send and send comp all requests.\n");
            goto out_deallocate_buffer;
        }

        print_sha1sum(requests, params.count);
        switch (params.mode) {
            case MODE_LATENCY:
                print_latency_report(&params, requests);
                break;
            case MODE_THROUGHPUT:
                print_throughput_report(&params, requests);
                break;
        }

        log_debug("\n");
    }

log_debug("Sending terminating message ...\n");
    char buf[4] = "End";
    if ((rv = send_message(id, buf, 4)) < 0) {
        log_error("Failed to send terminating message.\n");
        goto out_deallocate_buffer;
    }

out_deallocate_buffer:
    log_debug("de-allocating buffer\n");
    deallocate_buffer(requests, params.count);
out_disconnect:
    log_debug("Disconnecting\n");
    dccs_client_disconnect(id, res);
end:
    return rv;
}

