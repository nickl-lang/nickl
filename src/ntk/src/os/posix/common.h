#ifndef NTK_POSIX_COMMON_H_
#define NTK_POSIX_COMMON_H_

#include <unistd.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define handle_toNative nk_handleToVoidPtr
#define handle_fromNative nk_handleFromVoidPtr

NK_INLINE i32 handle_toFd(NkHandle handle) {
    return (i32)handle.val - 1;
}

NK_INLINE NkHandle handle_fromFd(i32 fd) {
    return (NkHandle){(intptr_t)(fd + 1)};
}

NK_INLINE pid_t handle_toPid(NkHandle handle) {
    return (pid_t)handle.val;
}

NK_INLINE NkHandle handle_fromPid(pid_t pid) {
    return (NkHandle){(intptr_t)pid};
}

#ifdef __cplusplus
}
#endif

#endif // NTK_POSIX_COMMON_H_
