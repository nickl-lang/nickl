#include "ntk/sys/time.h"

#include "ntk/time.h"

uint64_t nk_getTscFreq(void) {
    return nk_estimateTscFrequency();
}
