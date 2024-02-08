#include "ntk/time.h"

#include <time.h>
#include <x86intrin.h>

#include "ntk/sys/thread.h"
#include "ntk/sys/time.h"

int64_t nk_getTimeNs(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

uint64_t nk_estimateTscFrequency(void) {
    uint64_t tsc_freq = 0;

    // Get time before sleep
    uint64_t nsc_begin = 0;
    nsc_begin = nk_getTimeNs();
    uint64_t tsc_begin = __rdtsc();

    // 10ms gives ~4.5 digits of precision
    // the longer you sleep, the more precise you get
    nk_usleep(10000);

    // Get time after sleep
    uint64_t nsc_end = nsc_begin + 1;
    nsc_end = nk_getTimeNs();
    uint64_t tsc_end = __rdtsc();

    if (nsc_end > nsc_begin) {
        // Do the math to extrapolate the RDTSC ticks elapsed in 1 second
        tsc_freq = (tsc_end - tsc_begin) * 1000000000ull / (nsc_end - nsc_begin);
    } else {
        // Failure case
        tsc_freq = 1000000000;
    }

    return tsc_freq;
}
