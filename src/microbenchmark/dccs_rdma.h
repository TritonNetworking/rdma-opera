/**
 * RDMA wrapper library for DC circuit switch
 */

#ifndef DCCS_RDMA_H
#define DCCS_RDMA_H

#include <byteswap.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "dccs_parameters.h"
#include "dccs_utils.h"

#define SUCCESS 0
#define FAILURE -1

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

/* Connection setup/teardown */

int dccs_set_connection_tos(struct rdma_cm_id *id, uint8_t tos) {
log_debug("Setting tos to %hhu\n", tos);
    int rv = rdma_set_option(id, RDMA_OPTION_ID, RDMA_OPTION_ID_TOS,
                                &tos, sizeof(uint8_t));
    if (rv != 0) {
        log_perror("rdma_set_option");
    }

    return rv;
}

int check_add_port(const char *servername,
        const char* port,
        struct addrinfo *hints,
        struct addrinfo **res)
{
    int rv;
    if ((rv = getaddrinfo(servername, port, hints, res)) != 0) {
        fprintf(stderr, "%s for %s:%s\n", gai_strerror(rv), servername, port);
        return -1;
    }

    return 0;
}

int dccs_connect(struct dccs_connection *conn, char *server, char *port, uint8_t tos) {
    fprintf(stderr, "dccs_connect(): server = %s, port = %s.\n", server, port);

    conn->cm_channel = rdma_create_event_channel();
    if (conn->cm_channel == NULL) {
        fprintf(stderr, " rdma_create_event_channel failed\n");
        return FAILURE;
    }

    if (rdma_create_id(conn->cm_channel, &conn->cm_id, NULL, DEFAULT_PORT_SPACE) != 0) {
        fprintf(stderr,"rdma_create_id failed\n");
        return FAILURE;
    }

    struct sockaddr_in sin;
    struct addrinfo *res;
    struct rdma_cm_event *event;
    struct addrinfo hints;
    struct rdma_conn_param conn_param;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (check_add_port(server, port, &hints, &res) != 0) {
        fprintf(stderr, "Problem in resolving basic adress and port\n");
        return FAILURE;
    }

    memset(&sin, 0, sizeof sin);
    sin.sin_addr.s_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
    sin.sin_family = PF_INET;
    sin.sin_port = htons((unsigned short)atoi(port));

    fprintf(stderr, "Resolving address...\n");
    if (rdma_resolve_addr(conn->cm_id, NULL,(struct sockaddr *)&sin,2000)) {
        fprintf(stderr, "rdma_resolve_addr failed\n");
        return -1;
    }

    if (rdma_get_cm_event(conn->cm_channel,&event)) {
        fprintf(stderr, "rdma_get_cm_events failed\n");
        return -1;
    }

    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stderr, "unexpected CM event %d\n",event->event);
        rdma_ack_cm_event(event);
        return -1;
    }

    rdma_ack_cm_event(event);

    if (rdma_set_option(conn->cm_id, RDMA_OPTION_ID, RDMA_OPTION_ID_TOS, &tos, sizeof tos)) {
        fprintf(stderr, " Set TOS option failed: %d\n",event->event);
        return -1;
    }

    fprintf(stderr, "Resolving route...\n");
    if (rdma_resolve_route(conn->cm_id,2000)) {
        fprintf(stderr, "rdma_resolve_route failed\n");
        return -1;
    }

    if (rdma_get_cm_event(conn->cm_channel,&event)) {
        fprintf(stderr, "rdma_get_cm_events failed\n");
        return -1;
    }

    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stderr, "unexpected CM event %d\n",event->event);
        rdma_ack_cm_event(event);
        return -1;
    }

    rdma_ack_cm_event(event);

    fprintf(stderr, "Connecting ...\n");

    memset(&conn_param, 0, sizeof conn_param);
    conn_param.retry_count = 7;
    conn_param.rnr_retry_count = 7;

    if (rdma_connect(conn->cm_id, &conn_param)) {
        fprintf(stderr, "Function rdma_connect failed\n");
        perror("rdma_connect");
        return -1;
    }

    if (rdma_get_cm_event(conn->cm_channel,&event)) {
        fprintf(stderr, "rdma_get_cm_events failed\n");
        return -1;
    }

    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        rdma_ack_cm_event(event);
        fprintf(stderr, "Unexpected CM event bl blka %d\n", event->event);
        perror("rdma_connect");
        return -1;
    }

    rdma_ack_cm_event(event);
    return 0;
}

int dccs_listen(struct dccs_connection *conn, char *port) {
    fprintf(stderr, "dccs_listen(): port = %s.\n", port);

    conn->cm_channel = rdma_create_event_channel();
    if (conn->cm_channel == NULL) {
        fprintf(stderr, " rdma_create_event_channel failed\n");
        return FAILURE;
    }

    if (rdma_create_id(conn->cm_channel, &conn->cm_id_control, NULL, DEFAULT_PORT_SPACE) != 0) {
        fprintf(stderr,"rdma_create_id failed\n");
        return FAILURE;
    }


    struct sockaddr_in sin;
    struct addrinfo *res;
    struct rdma_cm_event *event;
    struct addrinfo hints;
    struct rdma_conn_param conn_param;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (check_add_port(NULL, port, &hints, &res) != 0) {
        fprintf(stderr, "Problem in resolving basic adress and port\n");
        return FAILURE;
    }

    memset(&sin, 0, sizeof sin);
    sin.sin_addr.s_addr = 0;
    sin.sin_family = PF_INET;
    sin.sin_port = htons((unsigned short)atoi(port));

    if (rdma_bind_addr(conn->cm_id_control, (struct sockaddr *)&sin)) {
        fprintf(stderr," rdma_bind_addr failed\n");
        return -1;
    }

    if (rdma_listen(conn->cm_id_control, 0)) {
        fprintf(stderr, "rdma_listen failed\n");
        return -1;
    }

    if (rdma_get_cm_event(conn->cm_channel, &event)) {
        fprintf(stderr, "rdma_get_cm_events failed\n");
        return -1;
    }

    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "bad event waiting for connect request %d\n", event->event);
        return -1;
    }

    conn->cm_id = (struct rdma_cm_id*)event->id;

    memset(&conn_param, 0, sizeof conn_param);
    //conn_param.retry_count = 7;
    //conn_param.rnr_retry_count = 7;

    if (rdma_accept(conn->cm_id, &conn_param)) {
    //if (rdma_accept(conn->cm_id, NULL)) {
        fprintf(stderr, "Function rdma_accept failed\n");
        perror("rdma_accept");
        return 1;
    }

    rdma_ack_cm_event(event);
    rdma_destroy_id(conn->cm_id_control);
    freeaddrinfo(res);

    return 0;
}

void dccs_client_disconnect(struct dccs_connection *conn) {
    rdma_destroy_id(conn->cm_id);
    rdma_destroy_event_channel(conn->cm_channel);
}

void dccs_server_disconnect(struct dccs_connection *conn) {
    rdma_destroy_id(conn->cm_id_control);
    rdma_destroy_event_channel(conn->cm_channel);
}

/* Memory Region registration */

struct ibv_mr * dccs_reg_msgs(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_msgs(id, addr, length)) == NULL) {
        log_perror("rdma_reg_msgs");
    }

    return mr;
}

struct ibv_mr * dccs_reg_read(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_read(id, addr, length)) == NULL) {
        log_perror("rdma_reg_read");
    }

    return mr;
}

struct ibv_mr * dccs_reg_write(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_write(id, addr, length)) == NULL) {
        log_perror("rdma_reg_write");
    }

    return mr;
}

void dccs_dereg_mr(struct ibv_mr *mr) {
    rdma_dereg_mr(mr);
}

/* RDMA Operations */

int dccs_rdma_send_with_flags(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr, int flags) {
    int rv;
    //flags = IBV_SEND_INLINE;    // TODO: check if possible
    //log_debug("RDMA send ...\n");
    if ((rv = rdma_post_send(id, NULL, addr, length, mr, flags)) != 0) {
        log_perror("rdma_post_send");
    }

    //log_debug("RDMA send returned %d.\n", rv);
    return rv;
}

static inline int dccs_rdma_send(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr) {
    return dccs_rdma_send_with_flags(id, addr, length, mr, IBV_SEND_SIGNALED);
}

int dccs_rdma_recv(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr) {
    int rv;
    //log_debug("RDMA recv ...\n");
    if ((rv = rdma_post_recv(id, NULL, addr, length, mr)) != 0) {
        log_perror("rdma_post_recv");
    }

    //log_debug("RDMA recv returned %d.\n", rv);
    return rv;
}

int dccs_rdma_read_with_flags(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey, int flags) {
    int rv;
    //log_debug("RDMA read ...\n");
    if ((rv = rdma_post_read(id, NULL, addr, length, mr, flags, remote_addr, rkey)) != 0) {
        log_perror("rdma_post_read");
    }

    //log_debug("RDMA read returned %d.\n", rv);
    return rv;
}

static inline int dccs_rdma_read(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey) {
    return dccs_rdma_read_with_flags(id, addr, length, mr, remote_addr, rkey, IBV_SEND_SIGNALED);
}

int dccs_rdma_write_with_flags(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey, int flags) {
    int rv;
    // log_debug("RDMA write ...\n");
    if ((rv = rdma_post_write(id, NULL, addr, length, mr, flags, remote_addr, rkey)) != 0) {
        log_perror("rdma_post_write");
    }

    // log_debug("RDMA write returned %d.\n", rv);
    return rv;
}

static inline int dccs_rdma_write(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey) {
    return dccs_rdma_write_with_flags(id, addr, length, mr, remote_addr, rkey, IBV_SEND_SIGNALED);
}

/* RDMA completion event */

/**
 * Retrieve a completed send, read or write request.
 */
int dccs_rdma_send_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    //log_debug("RDMA send completion ..\n");
    do {
        rv = ibv_poll_cq(id->send_cq, 1, wc);
    } while (rv == 0);

    if (rv < 0) {
        log_error("ibv_poll_cq() failed, error = %d.\n", rv);
        return -1;
    }

    if (wc->status != IBV_WC_SUCCESS) {
        log_error("Failed status %s (%d) for wr_id %d\n",
            ibv_wc_status_str(wc->status), wc->status, (int)wc->wr_id);
        return -1;
    }

    //log_debug("RDMA send completion returned %d.\n", rv);
    return rv;
}

/**
 * Retrieve a completed receive request.
 */
int dccs_rdma_recv_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    //log_debug("RDMA recv completion ..\n");
    do {
        rv = ibv_poll_cq(id->recv_cq, 1, wc);
    } while (rv == 0);

    if (rv < 0) {
        log_error("ibv_poll_cq() failed, error = %d.\n", rv);
        return -1;
    }

    if (wc->status != IBV_WC_SUCCESS) {
        log_error("Failed status %s (%d) for wr_id %d\n",
            ibv_wc_status_str(wc->status), wc->status, (int)wc->wr_id);
        return -1;
    }

    //log_debug("RDMA recv completion returned %d.\n", rv);
    return rv;
}

/* Manage multiple buffers. */

/**
 * Allocate and register multiple buffers.
 */
int allocate_buffer(struct rdma_cm_id *id, struct dccs_request *requests, struct dccs_parameters params) {
    Verb verb = params.verb;
    size_t count = params.count;
    size_t length = params.length;
    size_t count_per_mr = count / params.mr_count;
    size_t buffer_length = count_per_mr * length;

    struct ibv_mr *mr = NULL;
    void *buf_base = NULL;

    for (size_t n = 0; n < count; n++) {
        size_t offset = n % count_per_mr;
        if (offset == 0) {
            buf_base = malloc_random(buffer_length);

            switch (verb) {
                case Send:
                    mr = dccs_reg_msgs(id, buf_base, buffer_length);
                    break;
                case Read:
                    mr = dccs_reg_read(id, buf_base, buffer_length);
                    break;
                case Write:
                    mr = dccs_reg_write(id, buf_base, buffer_length);
                    break;
                default:
                    log_error("Unrecognized verb %d.\n", verb);
                    break;
            }
        }

        struct dccs_request *request = requests + n;
// perftest latency test: same buffer
#if 0
        offset = 0;
#endif
        void *buf = (void*)((uint8_t *)buf_base + offset * length);

        request->verb = verb;
        request->buf = buf;
        request->length = length;
        request->mr = mr;
    }

    return 0;
}

/**
 * De-allocate and de-register multiple buffers.
 */
void deallocate_buffer(struct dccs_request *requests, struct dccs_parameters params) {
    size_t count = params.count;
    size_t count_per_mr = count / params.mr_count;

    for (size_t n = 0; n < count; n++) {
        if (n % count_per_mr != 0)
            continue;

        struct dccs_request *request = requests + n;
        dccs_dereg_mr(request->mr);
        free(request->buf);
    }
}

/* Exchange MR information. */

/**
 * Get RDMA MR information from remote peer.
 */
int get_remote_mr_info(struct rdma_cm_id *id, struct dccs_request *requests, size_t count) {
    struct ibv_mr *mr_count, *mr_array;
    struct ibv_wc wc;
    int rv = -1;

    // TODO: only transmit one MR

#if VERBOSE_TIMING
    uint64_t t = get_cycles();
#endif
    // Allocate array
    size_t array_size = count * sizeof(struct dccs_mr_info);
    struct dccs_mr_info *mr_infos = malloc(array_size);
    memset(mr_infos, 0, array_size);
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to allocate MR structs: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    size_t rcount = 0;
    if ((mr_count = dccs_reg_msgs(id, &rcount, sizeof rcount)) == NULL)
        goto out_free_buf;
    if ((mr_array = dccs_reg_msgs(id, mr_infos, array_size)) == NULL)
        goto out_dereg_mr_count;
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to regiser MR infos: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    // Receive count
    if ((rv = dccs_rdma_recv(id, &rcount, sizeof rcount, mr_count)) != 0) {
        log_error("Failed to recv # of RDMA requests to remote side.\n");
        goto failure;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to recv comp # of RDMA requests to remote side.\n");
        goto failure;
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to receive count: %.3f µsec.\n", get_time_in_microseconds(t));
#endif

    if (rcount != count) {
        log_error("Inconsistent request count: local is %zu, remote is %zu.\n", count, rcount);
        goto failure;
    }

#if VERBOSE_TIMING
    t = get_cycles();
#endif
    // Receive RDMA read/write info
    if ((rv = dccs_rdma_recv(id, mr_infos, array_size, mr_array)) != 0) {
        log_error("Failed to recv RDMA read/write request info to remote side.\n");
        goto failure;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to recv comp RDMA read/write request info to remote side.\n");
        goto failure;
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to receive MR infos: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    // Process received info
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        struct dccs_mr_info *mr_info = mr_infos + n;
        request->remote_addr = ntohll(mr_info->addr);
        request->remote_rkey = ntohl(mr_info->rkey);
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to copy MR infos: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
failure:
// out_dereg_mr_array:
    dccs_dereg_mr(mr_array);
out_dereg_mr_count:
    dccs_dereg_mr(mr_count);
out_free_buf:
    free(mr_infos);

#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to clean up: %.3f µsec.\n", get_time_in_microseconds(t));
#endif

    return rv;
}

/**
 * Send RDMA MR information to remote peer.
 */
int send_local_mr_info(struct rdma_cm_id *id, struct dccs_request *requests, size_t count, size_t length) {
    struct ibv_mr *mr_count, *mr_array;
    struct ibv_wc wc;
    int rv = -1;

    // TODO: only transmit one MR

#if VERBOSE_TIMING
    uint64_t t = get_cycles();
#endif
    size_t array_size = count * sizeof(struct dccs_mr_info);
    struct dccs_mr_info *mr_infos = malloc(array_size);
    memset(mr_infos, 0, array_size);

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        struct dccs_mr_info *mr_info = mr_infos + n;
        mr_info->addr = htonll((uint64_t)request->buf);
        mr_info->rkey = htonl(request->mr->rkey);
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to allocate MR structs: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    if ((mr_count = dccs_reg_msgs(id, &count, sizeof count)) == NULL)
        goto out_free_buf;
    if ((mr_array = dccs_reg_msgs(id, mr_infos, array_size)) == NULL)
        goto out_dereg_mr_count;
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to register MR infos: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    if ((rv = dccs_rdma_send(id, &count, sizeof count, mr_count)) != 0) {
        log_error("Failed to send # of RDMA requests to remote side.\n");
        goto failure;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to send comp # of RDMA requests to remote side.\n");
        goto failure;
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to send count: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
    if ((rv = dccs_rdma_send(id, mr_infos, array_size, mr_array)) != 0) {
        log_error("Failed to send RDMA read/write request info to remote side.\n");
        goto failure;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to send comp RDMA read/write request info to remote side.\n");
        goto failure;
    }
#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to send MR infos: %.3f µsec.\n", get_time_in_microseconds(t));

    t = get_cycles();
#endif
failure:
// out_dereg_mr_array:
    dccs_dereg_mr(mr_array);
out_dereg_mr_count:
    dccs_dereg_mr(mr_count);
out_free_buf:
    free(mr_infos);

#if VERBOSE_TIMING
    t = get_cycles() - t;
    log_verbose("Time taken to clean up: %.3f µsec.\n", get_time_in_microseconds(t));
#endif

    return rv;
}

/* Simple wrapper for sending/receiving a single request */

int send_message(struct rdma_cm_id *id, void* buf, size_t length) {
    struct ibv_mr *mr;
    struct ibv_wc wc;
    int rv = -1;

    if ((mr = dccs_reg_msgs(id, buf, length)) == NULL)
        goto end;
    if ((rv = dccs_rdma_send(id, buf, length, mr)) != 0) {
        log_error("Failed to send message.\n");
        goto out_dereg_mr;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to send comp message.\n");
        goto out_dereg_mr;
    }

out_dereg_mr:
    dccs_dereg_mr(mr);
end:
    return rv;
}

int recv_message(struct rdma_cm_id *id, void* buf, size_t length) {
    struct ibv_mr *mr;
    struct ibv_wc wc;
    int rv = -1;

    if ((mr = dccs_reg_msgs(id, buf, length)) == NULL)
        goto end;
    if ((rv = dccs_rdma_recv(id, buf, length, mr)) != 0) {
        log_error("Failed to recv message.\n");
        goto out_dereg_mr;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        log_error("Failed to recv comp message.\n");
        goto out_dereg_mr;
    }

out_dereg_mr:
    dccs_dereg_mr(mr);
end:
    return rv;
}

/* Wrapper for sending/receiving multiple requests */

/**
 * Send multiple RDMA requests.
 */
int send_requests(struct rdma_cm_id *id, struct dccs_request *requests, size_t count) {
    int rv;
    int failed_count = 0;

    uint64_t start = get_cycles();

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        switch (request->verb) {
            case Send:
                rv = dccs_rdma_send(id, request->buf, request->length, request->mr);
                request->start = get_cycles();
                if (rv != 0)
                    failed_count++;
                break;
            case Read:
                rv = dccs_rdma_read(id, request->buf, request->length, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed_count++;
                break;
            case Write:
                rv = dccs_rdma_write(id, request->buf, request->length, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed_count++;
                break;
            default:
                log_warning("Unrecognized request (n = %zu).", n);
                break;
        }
    }

    uint64_t end = get_cycles();
    log_debug("Time elapsed to send all requests: %.3f µsec.\n", (double)(end - start) * 1e6 / 2.4e9);

    return -failed_count;
}

/**
 * Wait for multiple RDMA requests to finish.
 */
int wait_requests(struct rdma_cm_id *id, struct dccs_request *requests, size_t count) {
    int rv;
    int failed_count = 0;
    struct ibv_wc wc;

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        rv = dccs_rdma_send_comp(id, &wc);
        request->end = get_cycles();
        if (rv < 0)
            failed_count++;
    }

    return -failed_count;
}

int recv_requests(struct rdma_cm_id *id, struct dccs_request *requests, struct dccs_parameters *params) {
    int rv;
    int failed_count = 0;
    struct ibv_wc wc;

    for (size_t n = 0; n < params->count; n++) {
        struct dccs_request *request = requests + n;
        rv = dccs_rdma_recv(id, request->buf, request->length, request->mr);
        if (rv != 0) {
            log_error("Failed to recv messages.\n");
            ++failed_count;
        }
        while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
        if (rv < 0) {
            log_error("Failed to recv messages.\n");
            ++failed_count;
        }
    }

    return -failed_count;
}

/**
 * Send and wait for multiple RDMA requests.
 */
int send_and_wait_requests(struct rdma_cm_id *id, struct dccs_request *requests, struct dccs_parameters *params) {
    int rv;
    int failed_count = 0;
    size_t count = params->count;
    struct ibv_wc wc;
    int flags = 0;  // RDMA post signal

    switch (params->mode) {
        case MODE_LATENCY:      // Signal on all requests.
            flags |= IBV_SEND_SIGNALED;
            break;
        case MODE_THROUGHPUT:   // Only signal the last request.
            flags &= ~IBV_SEND_SIGNALED;
            break;
    }

    uint64_t start = get_cycles();

#if 0
    // Testing overhead
    log_debug("Testing overhead of get_cycles().\n");
    uint64_t *lats = malloc(count * sizeof(uint64_t));
    for (size_t n = 0; n < count; n++) {
        lats[n] = get_cycles() - start;
    }
    for (size_t n = 0; n < count; n++) {
        log_debug("n = %zu, clock = %zu.\n", n, lats[n]);
    }
    log_debug("\n");
    free(lats);
#endif

    for (size_t n = 0; n < count; n++) {
        // In latency test, the tool always uses the same request.
        // size_t offset = params->mode == MODE_LATENCY ? 0 : n;
        size_t offset = n;
        struct dccs_request *request = requests + offset;
        bool failed = false;
        if (n == count - 1) {   // Always signal the last request
            flags |= IBV_SEND_SIGNALED;
        }

#if 0
        log_verbose("buf = %p, length = %zu, remote = %p.\n", request->buf, request->length, request->remote_addr);
#endif

        switch (request->verb) {
            case Send:
                rv = dccs_rdma_send_with_flags(id, request->buf, request->length, request->mr, flags);
                break;
            case Read:
                rv = dccs_rdma_read_with_flags(id, request->buf, request->length, request->mr, request->remote_addr, request->remote_rkey, flags);
                break;
            case Write:
                rv = dccs_rdma_write_with_flags(id, request->buf, request->length, request->mr, request->remote_addr, request->remote_rkey, flags);
                break;
            default:
                log_warning("Unrecognized request (n = %zu).", n);
                rv = 0;
                break;
        }

        requests[n].start = get_cycles();
        if (rv != 0)
            failed = true;

        if (flags & IBV_SEND_SIGNALED) {
            rv = dccs_rdma_send_comp(id, &wc);
            requests[n].end = get_cycles();
            if (rv < 0)
                failed = true;
        }

        if (failed)
            failed_count++;
    }

    uint64_t end = get_cycles();

// perftest latency test: use previous end as start
#if 0
    for (size_t n = 0; n < count - 1; n++) {
        requests[n].end = requests[n + 1].start;
    }

    requests[count - 1].end = end;
#endif

#if VERBOSE_TIMING
    for (size_t n = 0; n < count; n++) {
        log_verbose("n = %zu, start = %zu, end = %zu, elapsed = %zu.\n", n, requests[n].start - start, requests[n].end - start, requests[n].end - requests[n].start);
    }
#endif

    log_debug("Time elapsed to send and wait all requests: %.3f µsec.\n", (double)(end - start) * 1e6 / (double)clock_rate);

    return -failed_count;
}

/* Reporting functions */

void print_sha1sum(struct dccs_request *requests, size_t count) {
    if (count == 0) {
        log_error("Failed to calculate SHA1 sum: empty request array.");
        return;
    }

    unsigned char digest[SHA_DIGEST_LENGTH];

    size_t length = requests[0].length;
    void **array = malloc(count * sizeof(void *));
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        array[n] = request->buf;
    }

    sha1sum_array((const void **)array, count, length, digest);
    char *digest_hex = bin_to_hex_string(digest, SHA_DIGEST_LENGTH);
    log_info("SHA1 sum: count = %zu, length = %zu, digest = %s.\n", count, length, digest_hex);

    free(digest_hex);
    free(array);
}

void print_raw_latencies(double *latencies, size_t count) {
    log_verbose("Raw latency (µsec):\n");
    log_verbose("Start,End,Latency\n");
    for (size_t n = 0; n < count; n++)
        log_verbose("%.3f\n", latencies[n]);

    log_verbose("\n");
}

/**
 * Print latency report.
 */
void print_latency_report(struct dccs_parameters *params, struct dccs_request *requests) {
    double sum = 0;
    double min = DBL_MAX;
    double max = 0;
    double median, average, stdev, sumsq;
    double percent90, percent99;

    // Note: latency measurement does not take warmup into account for now.
    size_t count = params->count;
    size_t length = params->length;

    double *latencies = malloc(count * sizeof(double));

    log_info("=====================\n");
    log_info("Latency Report\n");

    double first_start = (double)requests[0].start * MILLION / (double)clock_rate;
    int finished_count = 0;

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        uint64_t elapsed_cycles = request->end - request->start;
        //double start = (double)request->start * MILLION / (double)clock_rate;
        double end = (double)request->end * MILLION / (double)clock_rate;
        double latency = (double)elapsed_cycles * MILLION / (double)clock_rate;
        if (end - first_start <= DCCS_CYCLE_UPTIME)
            finished_count++;

        latencies[n] = latency;
        sum += latency;
        if (latency > max)
            max = latency;
        if (latency < min)
            min = latency;
    }

    if (params->verbose)
        print_raw_latencies(latencies, count);

    sort_latencies(latencies, count);
    median = latencies[count / 2];
    percent90 = latencies[(int)((double)count * 0.9)];
    percent99 = latencies[(int)((double)count * 0.99)];
    average = sum / (double)count;

    sumsq = 0;
    for (size_t n = 0; n < count; n++) {
        double latency = latencies[n];
        sumsq += pow((latency - average), 2);
    }

    stdev = sqrt(sumsq / (double)count);

    log_info("#bytes, #iterations, median, average, min, max, stdev, percent90, percent99\n");
    log_info("%zu, %zu, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", length, count, median, average, min, max, stdev, percent90, percent99);
    log_info("# of requests sent in %d µsec: %d.\n", DCCS_CYCLE_UPTIME, finished_count);
    log_info("=====================\n\n");

    free(latencies);
}

/**
 * Print throughput report.
 */
void print_throughput_report(struct dccs_parameters *params, struct dccs_request *requests) {
    size_t warmup_count = params->warmup_count;
    size_t count = params->count;
    size_t length = params->length;

    size_t transfered_bytes = (count - warmup_count) * length;
    uint64_t start_cycles = requests[warmup_count].start;
    uint64_t end_cycles = requests[count - 1].end;
    double elapsed_seconds = (double)(end_cycles - start_cycles) / (double)clock_rate;
    double throughput_bytes_per_second = (double)transfered_bytes / elapsed_seconds;
    double throughput_gbits = throughput_bytes_per_second * 8 / 1e9;

    log_info("=====================\n");
    log_info("Throughput Report\n");
    log_info("Transferred: %lu B, elapsed: %.3e s, throughput: %.3f Gbps.\n", transfered_bytes, elapsed_seconds, throughput_gbits);
    log_info("=====================\n\n");
}

#endif // DCCS_RDMA_H
