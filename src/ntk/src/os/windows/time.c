#include "ntk/time.h"

#include <unistd.h>
#include <x86intrin.h>

#include "ntk/time.h"

u64 nk_getTscFreq(void) {
    return nk_estimateTscFrequency();
}

void nk_usleep(u64 usec) {
    usleep(usec);
}

u64 nk_readTsc(void) {
    return __rdtsc();
}
