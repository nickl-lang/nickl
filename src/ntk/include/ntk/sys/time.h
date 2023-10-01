#ifndef HEADER_GUARD_NTK_SYS_TIME
#define HEADER_GUARD_NTK_SYS_TIME

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t nktime_t;

nktime_t nk_getTimeNs(void);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_TIME
