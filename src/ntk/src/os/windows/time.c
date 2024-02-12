#include "ntk/os/time.h"

#include <unistd.h>

#include "ntk/time.h"

u64 nk_getTscFreq(void) {
    return nk_estimateTscFrequency();
}

void nk_usleep(u64 usec) {
    usleep(usec);
}
