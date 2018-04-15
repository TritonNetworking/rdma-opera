/**
 * Parameters for RDMA.
 */

#ifndef DCCS_PARAMETER
#define DCCS_PARAMETER

typedef enum { Read, Write } Verb;

struct dccs_parameters {
    Verb verb;
    
};

#endif // DCCS_PARAMETER
