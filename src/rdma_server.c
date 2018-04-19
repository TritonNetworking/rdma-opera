// RDMA Server

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

char *port = "1234";

uint64_t clock_rate = 0;

int main(int argc, char *argv[]) {
    struct rdma_cm_id *listen_id, *id;
    struct rdma_addrinfo *res;
    struct dccs_request *requests;
    size_t requests_count = MESSAGE_COUNT;
    size_t requests_length = MESSAGE_LENGTH;
    int rv = 0;

    clock_rate = get_clock_rate();
    debug("Clock rate = %lu.", clock_rate);

    if ((rv = dccs_listen(&listen_id, &id, &res, port)) != 0)
        goto end;

    size_t requests_size = requests_count * sizeof(struct dccs_request);
    requests = malloc(requests_size);
    memset(requests, 0, requests_size);
    if ((rv = allocate_buffer(id, requests, requests_length, requests_count, Read)) != 0) {
        sys_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

    if ((rv = send_local_mr_info(id, requests, requests_count)) < 0) {
        sys_error("Failed to get remote MR info.\n");
        goto out_deallocate_buffer;
    }

    char buf[4] = { 0 };
    if ((rv = recv_message(id, buf, 4)) < 0) {
        sys_error("Failed to recv terminating message.\n");
        goto out_deallocate_buffer;
    }

out_deallocate_buffer:
    debug("de-allocating buffer\n");
    deallocate_buffer(requests, requests_count);
out_disconnect:
    debug("Disconnecting\n");
    dccs_server_disconnect(id, listen_id, res);
end:
    return rv;
}

