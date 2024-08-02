#ifndef NTK_OS_ERROR_H_
#define NTK_OS_ERROR_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32 nkerr_t;

NK_EXPORT nkerr_t nk_getLastError(void);
NK_EXPORT void nk_setLastError(nkerr_t err);

NK_EXPORT char const *nk_getErrorString(nkerr_t err);

NK_INLINE char const *nk_getLastErrorString(void) {
    return nk_getErrorString(nk_getLastError());
}

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_ERROR_H_
