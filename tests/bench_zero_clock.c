/* SPDX-License-Identifier: Apache-2.0 */
/* Compile the real benchmark with a deterministic clock-resolution boundary. */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
static int frozen_clock(clockid_t clock_id, struct timespec *t) {
    (void)clock_id;
    t->tv_sec=1234;
    t->tv_nsec=0;
    return 0;
}
#define clock_gettime frozen_clock
#include "../benchmarks/bench.c"
