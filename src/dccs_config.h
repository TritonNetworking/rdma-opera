/**
 * Configurations.
 */

#ifndef DCCS_CONFIG_H
#define DCCS_CONFIG_H

/* Machine configuration */
#define CPU_CLOCK_RATE 2400000000   // 2.4 GHz
#define CACHE_LINE_SIZE 64

/* Protocol default values */
#define DEFAULT_MESSAGE_COUNT 1000
#define DEFAULT_MESSAGE_LENGTH 2
#define DEFAULT_PORT "1234"

/* Protocol configuration */
#define DCCS_CYCLE_UPTIME 180   // Cycle up time, in µsec
#define DCCS_CYCLE_DOWNTIME 20  // Cycle down time, in µsec

#endif // DCCS_CONFIG_H

