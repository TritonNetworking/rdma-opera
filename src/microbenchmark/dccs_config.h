/**
 * Configurations.
 */

#ifndef DCCS_CONFIG_H
#define DCCS_CONFIG_H

#include "dccs_parameters.h"

/* Program settings */
#define DEBUG 1
#define VERBOSE_TIMING 0

/* Machine configuration */
// #define CPU_CLOCK_RATE 2400000000   // 2.4 GHz
#define CACHE_LINE_SIZE 64
#define USE_RDTSC 0
#define CPU_TO_USE 0

/* RDMA configuration */
#define MAX_WR 1000

/* Protocol default values */
#define DEFAULT_MESSAGE_COUNT 1000
#define DEFAULT_MESSAGE_LENGTH 2
#define DEFAULT_PORT "1234"
#define DEFAULT_WARMUP_COUNT 0
#define DEFAULT_MR_COUNT 1
#define DEFAULT_REPEAT_COUNT 1
#define DEFAULT_DIRECTION DIR_OUT
#define DEFAULT_TOS 0

/* Protocol configuration */
#define DCCS_CYCLE_UPTIME 180   // Cycle up time, in µsec
#define DCCS_CYCLE_DOWNTIME 20  // Cycle down time, in µsec
#define SYNC_END_MESSAGE "End"
#define SYNC_END_MESSAGE_LENGTH 4
#define MPI_FIRE_AND_FORGET 1

/* Math constants */
#define MILLION 1000000UL
#define BILLION 1000000000UL

#endif // DCCS_CONFIG_H

