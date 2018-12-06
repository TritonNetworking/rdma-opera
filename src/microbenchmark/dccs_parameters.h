/**
 * Parameters for RDMA.
 */

#ifndef DCCS_PARAMETER
#define DCCS_PARAMETER

#include <stdbool.h>
#include <stdint.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

typedef enum { None, Send, Read, Write } Verb;
typedef enum { MODE_LATENCY, MODE_THROUGHPUT } Mode;
typedef enum { DIR_OUT, DIR_IN, DIR_BOTH } Direction;
typedef enum { ROLE_CLIENT, ROLE_SERVER } Role;

struct dccs_mr_info{
    uint64_t addr;
    uint32_t rkey;
};

struct dccs_parameters {
    Verb verb;
    size_t count;
    size_t length;
    size_t repeat;
    char *server;
    char *port;

    Mode mode;
    size_t warmup_count;
    size_t mr_count;
    int direction;
    uint16_t index;
    uint8_t tos;
    uint8_t slot;
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

