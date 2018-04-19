/**
 * Utility functions.
 */

#ifndef DCCS_UTIL_H
#define DCCS_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define DEBUG 1
#define debug(...) if (DEBUG) fprintf(stderr, __VA_ARGS__)
#define sys_error(...) fprintf(stderr, __VA_ARGS__)
#define sys_warning(...) fprintf(stderr, __VA_ARGS__)

void debug_bin(void *buf, size_t length) {
#if DEBUG
    char *c;
    for (size_t i = 0; i < length; i++) {
        c = buf + i;
        fprintf(stderr, "%02x", *c);
    }
#endif
}

#if defined (__x86_64__) || defined(__i386__)
/* Note: only x86 CPUs which have rdtsc instruction are supported. */
static inline uint64_t get_cycles()
{
    unsigned low, high;
    unsigned long long val;
    asm volatile ("rdtsc" : "=a" (low), "=d" (high));
    val = high;
    val = (val << 32) | low;
    return val;
}

uint64_t get_clock_rate() {
  unsigned start_low, start_high, end_low, end_high;
  uint64_t start, end;

  asm volatile ("CPUID\n\t"
      "RDTSC\n\t"
      "mov %%edx, %0\n\t"
      "mov %%eax, %1\n\t": "=r" (start_high), "=r" (start_low)::"%rax", "%rbx", "%rcx", "%rdx"
  );

  sleep(1);

  asm volatile ("RDTSCP\n\t"
      "mov %%edx, %0\n\t"
      "mov %%eax, %1\n\t"
      "CPUID\n\t": "=r" (end_high), "=r" (end_low)::"%rax", "%rbx", "%rcx", "%rdx"
  );

  start = ((uint64_t) start_high) << 32 | start_low;
  end = ((uint64_t) end_high) << 32 | end_low;

  if (start >= end) {
    fprintf(stderr, "Invalid rdtsc/rdtscp results %lu, %lu.", end, start);
    exit(EXIT_FAILURE);
  } else {
    return end - start;
  }
}

#endif

#endif // DCCS_UTIL_H

