/**
 * RDMA wrapper library for DC circuit switch
 */

#ifndef DCCS_RDMA_H
#define DCCS_RDMA_H

#include <errno.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#define MAX_WR 1000

/* Connection setup/teardown */

int dccs_connect(struct rdma_cm_id **id, struct rdma_addrinfo **res, char *server, char *port) {
    struct rdma_addrinfo hints;
    struct ibv_qp_init_attr attr;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_port_space = RDMA_PS_TCP;
    if ((rv = rdma_getaddrinfo(server, port, &hints, res)) != 0) {
        perror("rmda_getaddrinfo");
        // fprintf(stderr, "rmda_getaddrinfo: %s\n", gai_strerror(rv));
        return rv;
    }

    memset(&attr, 0, sizeof attr);
    attr.cap.max_send_wr = attr.cap.max_recv_wr = MAX_WR;
    attr.qp_context = *id;
    attr.qp_type = IBV_QPT_RC;

    if ((rv = rdma_create_ep(id, *res, NULL, &attr)) != 0) {
        perror("rdma_create_ep");
    }

    if ((rv = rdma_connect(*id, NULL)) != 0) {
        perror("rdma_connect");
    }

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
    rdma_freeaddrinfo(res);
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
    if ((mr = rdma_reg_msgs(id, addr, length)) != NULL) {
        perror("rdma_reg_msgs");
    }

    return mr;
}

struct ibv_mr * dccs_reg_read(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_read(id, addr, length)) != NULL) {
        perror("rdma_reg_read");
    }

    return mr;
}

struct ibv_mr * dccs_reg_write(struct rdma_cm_id *id, void *addr, size_t length) {
    struct ibv_mr *mr;
    if ((mr = rdma_reg_write(id, addr, length)) != NULL) {
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
    if ((rv = rdma_post_send(id, NULL, addr, length, mr, flags)) != 0) {
        perror("rdma_post_send");
    }

    return rv;
}

int dccs_rdma_recv(struct rdma_cm_id *id, void *addr, size_t length, struct ibv_mr *mr) {
    int rv;
    if ((rv = rdma_post_recv(id, NULL, addr, length, mr)) != 0) {
        perror("rdma_post_recv");
    }

    return rv;
}

int dccs_rdma_read(struct rdma_cm_id *id, struct ibv_mr *mr, uint64_t remote_addr) {
    int rv;
    int flags = 0;
    if ((rv = rdma_post_read(id, NULL, mr->addr, mr->length, mr, flags, remote_addr, mr->rkey)) != 0) {
        perror("rdma_post_read");
    }

    return rv;
}

/* RDMA completion event */

/**
 * Retrieve a completed send, read or write request.
 */
int dccs_rdma_send_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    if ((rv = rdma_get_send_comp(id, wc)) != 0) {
        perror("rdma_get_send_comp");
    }

    return rv;
}

/**
 * Retrieve a completed receive request.
 */
int dccs_rdma_recv_comp(struct rdma_cm_id *id, struct ibv_wc *wc) {
    int rv;
    if ((rv = rdma_get_recv_comp(id, wc)) != 0) {
        perror("rdma_get_recv_comp");
    }

    return rv;
}

#endif // DCCS_RDMA_H
