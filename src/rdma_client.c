// RDMA Client

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

char *server = "10.0.0.100";
char *port = "1234";

uint64_t clock_rate = 0;

int main(int argc, char *argv[]) {
    struct rdma_cm_id *id;
    struct rdma_addrinfo *res;
    struct dccs_request *requests;
    size_t requests_count = MESSAGE_COUNT;
    size_t requests_length = MESSAGE_LENGTH;
    int rv = 0;

    dccs_init();

    if ((rv = dccs_connect(&id, &res, server, port)) != 0)
        goto end;

debug("Allocating buffer ...\n");
    size_t requests_size = requests_count * sizeof(struct dccs_request);
    requests = malloc(requests_size);
    memset(requests, 0, requests_size);
    if ((rv = allocate_buffer(id, requests, requests_length, requests_count, Read)) != 0) {
        sys_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

debug("Getting remote MR info ...\n");
    if ((rv = get_remote_mr_info(id, requests, requests_count)) < 0) {
        debug("rv = %d.\n", rv);
        sys_error("Failed to get remote MR info.\n");
        goto out_deallocate_buffer;
    }

/*
debug("Sending RDMA requests ...\n");
    if ((rv = send_requests(id, requests, requests_count)) < 0) {
        sys_error("Failed to send all requests.\n");
        goto out_deallocate_buffer;
    }

debug("Waiting for RDMA requests completion.\n");
    if ((rv = wait_requests(id, requests, requests_count)) < 0) {
        sys_error("Failed to send comp all requests.\n");
        goto out_deallocate_buffer;
    }
*/

debug("Sending and waiting for RDMA requests ...\n");
    if ((rv = send_and_wait_requests(id, requests, requests_count)) < 0) {
        sys_error("Failed to send and send comp all requests.\n");
        goto out_deallocate_buffer;
    }

debug("Sending terminating message ...\n");
    char buf[4] = "End";
    if ((rv = send_message(id, buf, 4)) < 0) {
        sys_error("Failed to send terminating message.\n");
        goto out_deallocate_buffer;
    }

    print_sha1sum(requests, requests_count);
    print_latency_report(requests, requests_count, requests_length, clock_rate);

out_deallocate_buffer:
    debug("de-allocating buffer\n");
    deallocate_buffer(requests, requests_count);
out_disconnect:
    debug("Disconnecting\n");
    dccs_client_disconnect(id, res);
end:
    return rv;
}

