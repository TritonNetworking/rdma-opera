// RDMA Client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "dccs_parameters.h"
#include "dccs_rdma.h"

char *server = "127.0.0.1";
char *port = "1234";

int main(int argc, char *argv[]) {
    struct rdma_cm_id *id;
    struct rdma_addrinfo *res;
    struct dccs_parameters params;
    struct ibv_mr *read_mr, *recv_mr;
    struct ibv_wc wc;
    int rv = 0;

    memset(&params, 0, sizeof params);

    if ((rv = dccs_connect(&id, &res, server, port)) != 0)
        goto end;

    size_t length = 16;
    void *buf = malloc(length);
    uint64_t remote_addr = 0;

    if ((recv_mr = dccs_reg_msgs(id, &remote_addr, sizeof remote_addr)) == NULL)
        goto out_free_buf;
    if ((rv = dccs_rdma_recv(id, &remote_addr, sizeof remote_addr, recv_mr)) != 0)
        goto out_dereg_recv_mr;
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_recv_mr;
    if (remote_addr == 0) {
        fprintf(stderr, "Invalid remote address: %lu.", remote_addr);
        rv = -1;
        goto out_dereg_recv_mr;
    }

    if ((read_mr = dccs_reg_read(id, buf, sizeof buf)) == NULL)
        goto out_dereg_recv_mr;
    if ((rv = dccs_rdma_read(id, read_mr, remote_addr)) != 0)
        goto out_dereg_read_mr;
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_read_mr;

    printf("Received: %s\n", (char *)buf);
    printf("End of operations\n");

out_dereg_read_mr:
    dccs_dereg_mr(read_mr);
out_dereg_recv_mr:
    dccs_dereg_mr(recv_mr);
out_free_buf:
    free(buf);
// out_disconnect:
    dccs_client_disconnect(id, res);
end:
    return rv;
}
