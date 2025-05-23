#ifndef NTK_TIME_H_
#define NTK_TIME_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT i64 nk_now_ns(void);

// Time Stamp Counter
NK_EXPORT u64 nk_getTscFreq(void);
NK_EXPORT u64 nk_estimateTscFrequency(void);
NK_EXPORT u64 nk_readTsc(void);

NK_EXPORT void nk_usleep(u64 usec);

#ifdef __cplusplus
}
#endif

#endif // NTK_TIME_H_
