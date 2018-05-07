/**
 * Parameters for RDMA.
 */

#ifndef DCCS_PARAMETER
#define DCCS_PARAMETER

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

typedef enum { None, Send, Read, Write } Verb;
typedef enum { MODE_LATENCY, MODE_THROUGHPUT } Mode;

struct dccs_mr_info{
    uint64_t addr;
    uint32_t rkey;
};

struct dccs_parameters {
    Verb verb;
    size_t count;
    size_t length;
    char *server;
    char *port;

    Mode mode;
    bool verbose;
};

struct dccs_request {
    Verb verb;

    // MR information
    struct ibv_mr *mr;
    void *buf;
    size_t length;

    // Remote address and rkey for RDMA read/write
    uint64_t remote_addr;
    uint32_t remote_rkey;

    // Timing information
    uint64_t start;
    uint64_t end;
};

#endif // DCCS_PARAMETER

