#include "ntk/sys/time.h"

#include "ntk/time.h"

u64 nk_getTscFreq(void) {
    return nk_estimateTscFrequency();
}
