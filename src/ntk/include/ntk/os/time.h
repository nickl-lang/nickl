#ifndef NTK_OS_TIME_H_
#define NTK_OS_TIME_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Time Stamp Counter frequency
NK_EXPORT u64 nk_getTscFreq(void);

NK_EXPORT void nk_usleep(u64 usec);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_TIME_H_
