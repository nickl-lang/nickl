#include "ntk/time.h"

#include <time.h>
#include <x86intrin.h>

#include "ntk/time.h"

i64 nk_now_ns(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

u64 nk_estimateTscFrequency(void) {
    u64 tsc_freq = 0;

    // Get time before sleep
    u64 nsc_begin = 0;
    nsc_begin = nk_now_ns();
    u64 tsc_begin = __rdtsc();

    // 10ms gives ~4.5 digits of precision
    // the longer you sleep, the more precise you get
    nk_usleep(10000);

    // Get time after sleep
    u64 nsc_end = nsc_begin + 1;
    nsc_end = nk_now_ns();
    u64 tsc_end = __rdtsc();

    if (nsc_end > nsc_begin) {
        // Do the math to extrapolate the RDTSC ticks elapsed in 1 second
        tsc_freq = (tsc_end - tsc_begin) * 1000000000ull / (nsc_end - nsc_begin);
    } else {
        // Failure case
        tsc_freq = 1000000000;
    }

    return tsc_freq;
}
