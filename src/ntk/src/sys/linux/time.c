#include "ntk/sys/time.h"

#include <time.h>

nktime_t nk_getTimeNs(void) {
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
