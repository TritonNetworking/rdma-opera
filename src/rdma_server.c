// RDMA Server

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

char *port = "1234";

int main(int argc, char *argv[]) {
    struct rdma_cm_id *listen_id, *id;
    struct rdma_addrinfo *res;
    struct ibv_mr *read_mr, *send_mr;
    struct ibv_wc wc;
    int rv = 0;

    if ((rv = dccs_listen(listen_id, id, res, port)) != 0)
        goto end;

    size_t length = 16;
    void *buf = malloc(length);
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "Hello world.");
    uint64_t addr = (uint64_t)buf;

    if ((send_mr = dccs_reg_msgs(id, &addr, sizeof addr)) == NULL)
        goto out_free_buf;
    if ((rv = dccs_rdma_send(id, &addr, sizeof addr, send_mr)) != 0) {
        goto out_dereg_send_mr;
    }
    while ((rv = dccs_rdma_send_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_send_mr;

    if ((read_mr = dccs_reg_read(id, buf, length)) == NULL)
        goto out_dereg_send_mr;
    while ((rv = dccs_rdma_recv_comp(id, &wc)) == 0);
    if (rv < 0)
        goto out_dereg_read_mr;

    printf("End of operations\n");

out_dereg_read_mr:
    dccs_dereg_mr(read_mr);
out_dereg_send_mr:
    dccs_dereg_mr(send_mr);
out_free_buf:
    free(buf);
out_disconnect:
    dccs_server_disconnect(id, listen_id, res);
end:
    return rv;
}
