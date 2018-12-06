// RDMA tool

#define _GNU_SOURCE

#define TEST_RDMA_SYNC 0

#include <stdio.h>

#include "dccs_parameters.h"
#include "dccs_utils.h"
#include "dccs_rdma.h"

uint64_t clock_rate = 0;    // Clock ticks per second

int run(struct dccs_parameters params) {
    struct rdma_cm_id *listen_id = NULL, *id;
    struct dccs_request *requests;
    int rv = 0;

    Role role = params.server == NULL ? ROLE_SERVER : ROLE_CLIENT;
    if (role == ROLE_CLIENT)
        log_info("Running in client mode ...\n");
    else
        log_info("Running in server mode ...\n");

    if (role == ROLE_CLIENT) {
        if ((rv = dccs_connect(&id, params.server, params.port, params.tos)) != 0)
            goto end;
    } else {    // role == ROLE_SERVER
        if ((rv = dccs_listen(&listen_id, &id, params.port)) != 0)
            goto end;
    }

    log_debug("Allocating buffer ...\n");
    size_t requests_size = params.count * sizeof(struct dccs_request);
    requests = malloc(requests_size);
    memset(requests, 0, requests_size);
    if ((rv = allocate_buffer(id, requests, params)) != 0) {
        log_error("Failed to allocate buffers.\n");
        goto out_disconnect;
    }

#if TEST_RDMA_SYNC
    uint8_t slot = 0;
    strcpy(requests[0].buf, "dummy dummy dummy");
    strcpy((char *)requests[0].buf + 18, "sync");
    memcpy((char *)requests[0].buf + 22, &slot, sizeof slot);
#endif

    if (params.verb == Read || params.verb == Write) {
        if (role == ROLE_CLIENT) {
            log_debug("Getting remote MR info ...\n");
            rv = get_remote_mr_info(id, requests, params.count);
            if (rv < 0) {
                log_debug("rv = %d.\n", rv);
                log_error("Failed to get remote MR info.\n");
                goto out_deallocate_buffer;
            }
        } else {    // role == ROLE_SERVER
            log_debug("Sending local MR info ...\n");
            rv = send_local_mr_info(id, requests, params.count);
            if (rv < 0) {
                log_error("Failed to get remote MR info.\n");
                goto out_deallocate_buffer;
            }
        }
    }

    for (size_t n = 0; n < params.repeat; n++) {
        log_info("Round %zu.\n", n + 1);

        if (role == ROLE_CLIENT) {
            // Client is active in RDMA experiments, i.e. requester.

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

            log_info("Sending and waiting for RDMA requests ...\n");
            if ((rv = send_and_wait_requests(id, requests, &params)) < 0) {
                log_error("Failed to send and send comp all requests.\n");
                goto out_end_request;
            }
        } else {    // role == ROLE_SERVER
            switch (params.verb) {
                case Read:
                case Write:
                    // Server is passive in RDMA experiments, i.e. responder.
                    break;
                case Send:
                    if ((rv = recv_requests(id, requests, &params)) < 0) {
                        log_error("Failed to receive all requests.\n");
                        goto out_end_request;
                    }

                    break;
                default:
                    log_warning("Unrecognized verb on server side: %d.\n",
                                params.verb);
                    break;
            }
        }

out_end_request:

        if (role == ROLE_CLIENT) {
            switch (params.mode) {
                case MODE_LATENCY:
                    print_latency_report(&params, requests);
                    break;
                case MODE_THROUGHPUT:
                    print_throughput_report(&params, requests);
                    break;
            }
        }
    }

    // Synchronize end of a round
    if (role == ROLE_CLIENT) {
        log_debug("Sending terminating message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = SYNC_END_MESSAGE;
        if ((rv = send_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to send terminating message.\n");
            goto out_deallocate_buffer;
        }
    } else {    // role == ROLE_SERVER
        log_debug("Waiting for end message ...\n");
        char buf[SYNC_END_MESSAGE_LENGTH] = {0};
        if ((rv = recv_message(id, buf, SYNC_END_MESSAGE_LENGTH)) < 0) {
            log_error("Failed to recv terminating message.\n");
            goto out_deallocate_buffer;
        }
    }

    // Print stats
    print_sha1sum(requests, params.count);

out_deallocate_buffer:
    log_debug("de-allocating buffer\n");
    deallocate_buffer(requests, params);
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

