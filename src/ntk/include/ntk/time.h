#ifndef NTK_TIME_H_
#define NTK_TIME_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

i64 nk_now_ns(void);

u64 nk_estimateTscFrequency(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_TIME_H_
