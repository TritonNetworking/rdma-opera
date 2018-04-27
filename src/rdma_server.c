// RDMA Server

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "config.h"
#include "utils.h"
#include "dccs_parameters.h"
#include "dccs_rdma.h"

uint64_t clock_rate = 0;

int main(int argc, char *argv[]) {
    struct rdma_cm_id *listen_id, *id;
    struct rdma_addrinfo *res;
    struct dccs_request *requests;
    struct dccs_parameters params;
    int rv = 0;

    parse_args(argc, argv, &params);
    print_parameters(&params);
    dccs_init();

    if ((rv = dccs_listen(&listen_id, &id, &res, params.port)) != 0)
        goto end;

debug("Allocating buffer ...\n");
    size_t requests_size = params.count * sizeof(struct dccs_request);
    requests = malloc(requests_size);
    memset(requests, 0, requests_size);
    if ((rv = allocate_buffer(id, requests, params.length, params.count, Read)) != 0) {
        sys_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

debug("Sending local MR info ...\n");
    if ((rv = send_local_mr_info(id, requests, params.count)) < 0) {
        sys_error("Failed to get remote MR info.\n");
        goto out_deallocate_buffer;
    }

debug("Waiting for end message ...\n");
    char buf[4] = { 0 };
    if ((rv = recv_message(id, buf, 4)) < 0) {
        sys_error("Failed to recv terminating message.\n");
        goto out_deallocate_buffer;
    }

    print_sha1sum(requests, params.count);

out_deallocate_buffer:
    debug("de-allocating buffer\n");
    deallocate_buffer(requests, params.count);
out_disconnect:
    debug("Disconnecting\n");
    dccs_server_disconnect(id, listen_id, res);
end:
    return rv;
}

