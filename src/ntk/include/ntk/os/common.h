#ifndef NTK_OS_COMMON_H_
#define NTK_OS_COMMON_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    intptr_t val;
} NkOsHandle;

#define NK_OS_HANDLE_ZERO (NK_LITERAL(NkOsHandle) NK_ZERO_STRUCT)

NK_INLINE bool nkos_handleEqual(NkOsHandle lhs, NkOsHandle rhs) {
    return lhs.val == rhs.val;
}

NK_INLINE bool nkos_handleIsZero(NkOsHandle handle) {
    return nkos_handleEqual(handle, NK_OS_HANDLE_ZERO);
}

#ifdef __cplusplus

inline bool operator==(NkOsHandle lhs, NkOsHandle rhs) {
    return nkos_handleEqual(lhs, rhs);
}

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_COMMON_H_
