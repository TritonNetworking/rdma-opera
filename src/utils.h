/**
 * Utility functions.
 */

#ifndef DCCS_UTIL_H
#define DCCS_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define DEBUG 1
#define debug(...) if (DEBUG) fprintf(stderr, __VA_ARGS__)

void debug_bin(void *buf, size_t length) {
#if DEBUG
    char *c;
    for (size_t i = 0; i < length; i++) {
        c = buf + i;
        fprintf(stderr, "%02x", *c);
    }
#endif
}

#endif // DCCS_UTIL_H

