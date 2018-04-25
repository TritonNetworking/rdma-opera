/**
 * RDMA wrapper library for DC circuit switch
 */

#ifndef DCCS_RDMA_H
#define DCCS_RDMA_H

#include <byteswap.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include "utils.h"

#define MAX_WR 1000

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

int dccs_connect(struct rdma_cm_id **id, struct rdma_addrinfo **res, char *server, char *port) {
    struct rdma_addrinfo hints;
    struct ibv_qp_init_attr attr;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if ((rv = rdma_getaddrinfo(server, port, &hints, res)) != 0) {
        perror("rmda_getaddrinfo");
        goto end;
    }

    memset(&attr, 0, sizeof attr);
    attr.cap.max_send_wr = attr.cap.max_recv_wr = MAX_WR;
    attr.qp_context = *id;
    attr.qp_type = IBV_QPT_RC;

    if ((rv = rdma_create_ep(id, *res, NULL, &attr)) != 0) {
        perror("rdma_create_ep");
        goto out_free_addrinfo;
    }

    if ((rv = rdma_connect(*id, NULL)) != 0) {
        perror("rdma_connect");
        goto out_destroy_listen_ep;
    }

    return 0;

out_destroy_listen_ep:
    rdma_destroy_ep(*id);
out_free_addrinfo:
    rdma_freeaddrinfo(*res);
end:
    return rv;
}

int dccs_listen(struct rdma_cm_id **listen_id, struct rdma_cm_id **id, struct rdma_addrinfo **res, char *port) {
    struct rdma_addrinfo hints;
    struct ibv_qp_init_attr attr;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_flags = RAI_PASSIVE;
    hints.ai_port_space = RDMA_PS_TCP;
    if ((rv = rdma_getaddrinfo(NULL, port, &hints, res)) != 0) {
        perror("rmda_getaddrinfo");
        goto end;
    }

    memset(&attr, 0, sizeof attr);
    attr.cap.max_send_wr = attr.cap.max_recv_wr = MAX_WR;
    attr.qp_type = IBV_QPT_RC;
    
    if ((rv = rdma_create_ep(listen_id, *res, NULL, &attr)) != 0) {
        perror("rdma_create_ep");
        goto out_free_addrinfo;
    }

    if ((rv = rdma_listen(*listen_id, 0)) != 0) {
        perror("rdma_listen");
        goto out_destroy_listen_ep;
    }

    if ((rv = rdma_get_request(*listen_id, id)) != 0) {
        perror("rdma_get_request");
        goto out_destroy_listen_ep;
    }

    // need ibv_query_qp?

    if ((rv = rdma_accept(*id, NULL)) != 0) {
        perror("rdma_accept");
        goto out_destroy_accept_ep;
    }

    return 0;

out_destroy_accept_ep:
    rdma_destroy_ep(*id);
out_destroy_listen_ep:
    rdma_destroy_ep(*listen_id);
out_free_addrinfo:
    rdma_freeaddrinfo(*res);
end:
    return rv;
}

void dccs_client_disconnect(struct rdma_cm_id *id, struct rdma_addrinfo *res) {
    rdma_disconnect(id);
    rdma_destroy_ep(id);
    rdma_freeaddrinfo(res);
}

void dccs_server_disconnect(struct rdma_cm_id *id, struct rdma_cm_id *listen_id, struct rdma_addrinfo *res) {
    rdma_disconnect(id);
    rdma_destroy_ep(id);
    rdma_destroy_ep(listen_id);
    rdma_freeaddrinfo(res);
}

/* Memory Region registration */

struct ibv_mr * dccs_reg_msgs(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_msgs(id, addr, length)) == NULL) {
        perror("rdma_reg_msgs");
    }

    return mr;
}

struct ibv_mr * dccs_reg_read(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_read(id, addr, length)) == NULL) {
        perror("rdma_reg_read");
    }

    return mr;
}

struct ibv_mr * dccs_reg_write(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_write(id, addr, length)) == NULL) {
        perror("rdma_reg_write");
    }

    return mr;
}

void dccs_dereg_mr(struct ibv_mr *mr) {
    rdma_dereg_mr(mr);
}

/* RDMA Operations */

/*
int dccs_exchange(struct rdma_cm_id *id) {
    return -1;
}
 */

int dccs_rdma_send(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr) {
    int rv;
    int flags = 0; //IBV_SEND_INLINE;    // TODO: check if possible
    flags |= IBV_SEND_SIGNALED;
    //debug("RDMA send ...\n");
    if ((rv = rdma_post_send(id, NULL, addr, length, mr, flags)) != 0) {
        perror("rdma_post_send");
    }

    //debug("RDMA send returned %d.\n", rv);
    return rv;
}

int dccs_rdma_recv(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr) {
    int rv;
    //debug("RDMA recv ...\n");
    if ((rv = rdma_post_recv(id, NULL, addr, length, mr)) != 0) {
        perror("rdma_post_recv");
    }

    //debug("RDMA recv returned %d.\n", rv);
    return rv;
}

int dccs_rdma_read(struct rdma_cm_id *id, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey) {
    int rv;
    int flags = IBV_SEND_SIGNALED;
    //debug("RDMA read ...\n");
    if ((rv = rdma_post_read(id, NULL, mr->addr, mr->length, mr, flags, remote_addr, rkey)) != 0) {
        perror("rdma_post_read");
    }

    //debug("RDMA read returned %d.\n", rv);
    return rv;
}

int dccs_rdma_write(struct rdma_cm_id *id, struct ibv_mr *mr, uint64_t remote_addr, uint32_t rkey) {
    int rv;
    int flags = IBV_SEND_SIGNALED;
    debug("RDMA write ...\n");
    if ((rv = rdma_post_read(id, NULL, mr->addr, mr->length, mr, flags, remote_addr, rkey)) != 0) {
        perror("rdma_post_write");
    }

    debug("RDMA write returned %d.\n", rv);
    return rv;
}

/* RDMA completion event */

/**
 * Retrieve a completed send, read or write request.
 */
int dccs_rdma_send_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    //debug("RDMA send completion ..\n");
    if ((rv = rdma_get_send_comp(id, wc)) == -1) {
        perror("rdma_get_send_comp");
    }

    //debug("RDMA send completion returned %d.\n", rv);
    return rv;
}

/**
 * Retrieve a completed receive request.
 */
int dccs_rdma_recv_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    //debug("RDMA recv completion ..\n");
    if ((rv = rdma_get_recv_comp(id, wc)) == -1) {
        perror("rdma_get_recv_comp");
    }

    //debug("RDMA recv completion returned %d.\n", rv);
    return rv;
}

/* Manage multiple buffers. */

/**
 * Allocate and register multiple buffers.
 */
int allocate_buffer(struct rdma_cm_id *id, struct dccs_request *requests, size_t length, size_t count, Verb verb) {
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        void *buf = malloc(length);

        request->verb = verb;
        request->buf = buf;
        request->length = length;

        switch (verb) {
            case Send:
                request->mr = dccs_reg_msgs(id, buf, length);
                break;
            case Read:
                request->mr = dccs_reg_read(id, buf, length);
                break;
            case Write:
                request->mr = dccs_reg_write(id, buf, length);
                break;
            default:
                sys_error("Unrecognized verb for request %zu.\n", n);
                break;
        }

        if (request->mr == NULL)
            return -1;
    }

    return 0;
}

/**
 * De-allocate and de-register multiple buffers.
 */
void deallocate_buffer(struct dccs_request *requests, size_t count) {
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        if (request->mr != NULL)
            dccs_dereg_mr(request->mr);

        if (request->buf != NULL)
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
    int rv;

    // Receive count
    size_t rcount = 0;
    if ((mr_count = dccs_reg_msgs(id, &rcount, sizeof rcount)) == NULL)
        goto end;
    if ((rv = dccs_rdma_recv(id, &rcount, sizeof rcount, mr_count)) != 0) {
        sys_error("Failed to recv # of RDMA requests to remote side.\n");
        goto out_dereg_mr_count;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to recv comp # of RDMA requests to remote side.\n");
        goto out_dereg_mr_count;
    }

    if (rcount != count) {
        sys_error("Inconsistent request count: local is %zu, remote is %zu.\n", count, rcount);
        goto out_dereg_mr_count;
    }

    // Allocate array
    size_t array_size = count * sizeof(struct dccs_mr_info);
    struct dccs_mr_info *mr_infos = malloc(array_size);
    memset(mr_infos, 0, array_size);

    // Receive RDMA read/write info
    if ((mr_array = dccs_reg_msgs(id, mr_infos, array_size)) == NULL)
        goto out_free_buf;
    if ((rv = dccs_rdma_recv(id, mr_infos, array_size, mr_array)) != 0) {
        sys_error("Failed to recv RDMA read/write request info to remote side.\n");
        goto out_dereg_mr_array;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to recv comp RDMA read/write request info to remote side.\n");
        goto out_dereg_mr_array;
    }

    // Process received info
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        struct dccs_mr_info *mr_info = mr_infos + n;
        request->remote_addr = ntohll(mr_info->addr);
        request->remote_rkey = ntohl(mr_info->rkey);
    }

out_dereg_mr_array:
    dccs_dereg_mr(mr_array);
out_free_buf:
    free(mr_infos);
out_dereg_mr_count:
    dccs_dereg_mr(mr_count);
end:
    return rv;
}

/**
 * Send RDMA MR information to remote peer.
 */
int send_local_mr_info(struct rdma_cm_id *id, struct dccs_request *requests, size_t count) {
    struct ibv_mr *mr_count, *mr_array;
    struct ibv_wc wc;
    int rv;

    size_t array_size = count * sizeof(struct dccs_mr_info);
    struct dccs_mr_info *mr_infos = malloc(array_size);
    memset(mr_infos, 0, array_size);

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        struct dccs_mr_info *mr_info = mr_infos + n;
        mr_info->addr = htonll((uint64_t)request->mr->addr);
        mr_info->rkey = htonl(request->mr->rkey);
    }

    if ((mr_count = dccs_reg_msgs(id, &count, sizeof count)) == NULL)
        goto out_free_buf;
    if ((mr_array = dccs_reg_msgs(id, mr_infos, array_size)) == NULL)
        goto out_dereg_mr_count;

    if ((rv = dccs_rdma_send(id, &count, sizeof count, mr_count)) != 0) {
        sys_error("Failed to send # of RDMA requests to remote side.\n");
        goto out_dereg_mr_array;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to send comp # of RDMA requests to remote side.\n");
        goto out_dereg_mr_array;
    }

    if ((rv = dccs_rdma_send(id, mr_infos, array_size, mr_array)) != 0) {
        sys_error("Failed to send RDMA read/write request info to remote side.\n");
        goto out_dereg_mr_array;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to send comp RDMA read/write request info to remote side.\n");
        goto out_dereg_mr_array;
    }

out_dereg_mr_array:
    dccs_dereg_mr(mr_array);
out_dereg_mr_count:
    dccs_dereg_mr(mr_count);
out_free_buf:
    free(mr_infos);

    return rv;
}

/* Simple wrapper for sending/receiving a single request */

int send_message(struct rdma_cm_id *id, void* buf, size_t length) {
    struct ibv_mr *mr;
    struct ibv_wc wc;
    int rv;

    if ((mr = dccs_reg_msgs(id, buf, length)) == NULL)
        goto end;
    if ((rv = dccs_rdma_send(id, buf, length, mr)) != 0) {
        sys_error("Failed to send message.\n");
        goto out_dereg_mr;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to send comp message.\n");
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
    int rv;

    if ((mr = dccs_reg_msgs(id, buf, length)) == NULL)
        goto end;
    if ((rv = dccs_rdma_recv(id, buf, length, mr)) != 0) {
        sys_error("Failed to recv message.\n");
        goto out_dereg_mr;
    }
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0) {
        sys_error("Failed to recv comp message.\n");
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

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        switch (request->verb) {
            case Send:
                request->mr = dccs_reg_msgs(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            case Read:
                request->mr = dccs_reg_read(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            case Write:
                request->mr = dccs_reg_write(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            default:
                printf("Unrecognized request (n = %zu).", n);
                break;
        }
    }

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
                rv = dccs_rdma_read(id, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed_count++;
                break;
            case Write:
                rv = dccs_rdma_write(id, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed_count++;
                break;
            default:
                printf("Unrecognized request (n = %zu).", n);
                break;
        }
    }

    uint64_t end = get_cycles();
    printf("Time elapsed to send all requests: %.3f µsec.\n", (double)(end - start) * 1e6 / 2.4e9);

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

/**
 * Send and wait for multiple RDMA requests.
 */
int send_and_wait_requests(struct rdma_cm_id *id, struct dccs_request *requests, size_t count) {
    int rv;
    int failed_count = 0;
    struct ibv_wc wc;

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        switch (request->verb) {
            case Send:
                request->mr = dccs_reg_msgs(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            case Read:
                request->mr = dccs_reg_read(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            case Write:
                request->mr = dccs_reg_write(id, request->buf, request->length);
                if (request->mr == NULL)
                    failed_count++;
                break;
            default:
                printf("Unrecognized request (n = %zu).", n);
                break;
        }
    }

    uint64_t start = get_cycles();

    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        bool failed = false;

        switch (request->verb) {
            case Send:
                rv = dccs_rdma_send(id, request->buf, request->length, request->mr);
                request->start = get_cycles();
                if (rv != 0)
                    failed = true;
                break;
            case Read:
                rv = dccs_rdma_read(id, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed = true;
                break;
            case Write:
                rv = dccs_rdma_write(id, request->mr, request->remote_addr, request->remote_rkey);
                request->start = get_cycles();
                if (rv != 0)
                    failed = true;
                break;
            default:
                printf("Unrecognized request (n = %zu).", n);
                break;
        }

        rv = dccs_rdma_send_comp(id, &wc);
        request->end = get_cycles();
        if (rv < 0)
            failed = true;

        if (failed)
            failed_count++;
    }

    uint64_t end = get_cycles();
    printf("Time elapsed to send and wait all requests: %.3f µsec.\n", (double)(end - start) * 1e6 / 2.4e9);

    return -failed_count;
}

/* Reporting functions */

/**
 * Print latency report.
 */
void print_latency_report(struct dccs_request *requests, size_t count, uint64_t clock_rate) {
    double sum = 0;
    double min = DBL_MAX;
    double max = 0;
    double median, average;

    bool verbose = true;
    double *latencies = malloc(count * sizeof(double));

    printf("\n=====================\n");
    printf("Report\n\n");

    printf("Raw latency (µsec):\n");
    printf("Start,End,Latency\n");
    for (size_t n = 0; n < count; n++) {
        struct dccs_request *request = requests + n;
        uint64_t elapsed_cycles = request->end - request->start;
        double start = (double)request->start * 1e6 / clock_rate;
        double end = (double)request->end * 1e6 / clock_rate;
        double latency = (double)elapsed_cycles * 1e6 / clock_rate;
        latencies[n] = latency;
        if (verbose)
            printf("%.3f,%.3f,%.3f\n", start, end, latency);

        sum += latency;
        if (latency > max)
            max = latency;
        if (latency < min)
            min = latency;
    }

    sort_latencies(latencies, count);
    median = latencies[count / 2];
    average = sum / count;

    printf("\n");
    printf("Configuration: request length: %zu, # of requests: %zu.\n", requests->length, count);
    printf("Stats: median: %.3f µsec, average: %.3f, min: %.3f µsec, max: %.3f µsec.\n", median, average, min, max);
    printf("=====================\n\n");

    free(latencies);
}

#endif // DCCS_RDMA_H
