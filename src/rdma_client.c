// RDMA Client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "utils.h"
#include "dccs_parameters.h"
#include "dccs_rdma.h"

char *server = "10.0.0.100";
char *port = "1234";

int main(int argc, char *argv[]) {
    struct rdma_cm_id *id;
    struct rdma_addrinfo *res;
    struct dccs_conn_param local_conn, remote_conn;
    struct ibv_mr *read_mr, *send_mr, *recv_mr;
    struct ibv_wc wc;
    uint64_t remote_addr;
    uint32_t remote_rkey;
    int rv = 0;

    memset(&local_conn, 0, sizeof local_conn);
    memset(&remote_conn, 0, sizeof remote_conn);

    if ((rv = dccs_connect(&id, &res, server, port)) != 0)
        goto end;

    size_t length = 32;
    void *buf = malloc(length);
    memset(buf, 0, length);

    if ((recv_mr = dccs_reg_msgs(id, &remote_conn, sizeof remote_conn)) == NULL)
        goto out_free_buf;
    if ((rv = dccs_rdma_recv(id, &remote_conn, sizeof remote_conn, recv_mr)) != 0)
        goto out_dereg_recv_mr;
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_recv_mr;

    remote_addr = ntohll(remote_conn.addr);
    remote_rkey = ntohl(remote_conn.rkey);
    debug("Successfully received remote connection parameters: addr = %p, rkey = %u.\n", (void *)remote_addr, remote_rkey);

    if ((read_mr = dccs_reg_read(id, buf, length)) == NULL)
        goto out_dereg_recv_mr;
    debug("buf = %p, addr = %p, lkey = %u, rkey = %u.\n", buf, read_mr->addr, read_mr->lkey, read_mr->rkey);
    read_mr->rkey = remote_rkey;
    debug("mr->length = %zu.\n", read_mr->length);

    if ((rv = dccs_rdma_read(id, read_mr, remote_addr, remote_rkey)) != 0)
        goto out_dereg_read_mr;
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_read_mr;

    debug("Received (bin): ");
    debug_bin(buf, length);
    debug("\n");
    debug("Received (ASCII): \"%s\"\n", (char *)buf);

    debug("End of operations\n");

out_dereg_read_mr:
    debug("De-reging read_mr\n");
    dccs_dereg_mr(read_mr);
out_dereg_recv_mr:
    debug("De-reging recv_mr\n");
    dccs_dereg_mr(recv_mr);
out_free_buf:
    debug("freeing buf\n");
    free(buf);
// out_disconnect:
    debug("Disconnecting\n");
    dccs_client_disconnect(id, res);
end:
    return rv;
}

