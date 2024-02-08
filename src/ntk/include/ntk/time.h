#ifndef HEADER_GUARD_NTK_TIME
#define HEADER_GUARD_NTK_TIME

#include <stdint.h>

int64_t nk_getTimeNs(void);

uint64_t nk_estimateTscFrequency(void);

#endif // HEADER_GUARD_NTK_TIME
