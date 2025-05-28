#ifndef NTK_POSIX_COMMON_H_
#define NTK_POSIX_COMMON_H_

#include <unistd.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define handle2native nk_handleToVoidPtr
#define native2handle nk_handleFromVoidPtr

NK_INLINE i32 handle2fd(NkHandle handle) {
    return (i32)handle.val - 1;
}

NK_INLINE NkHandle fd2handle(i32 fd) {
    return (NkHandle){(intptr_t)(fd + 1)};
}

NK_INLINE pid_t handle2pid(NkHandle handle) {
    return (pid_t)handle.val;
}

NK_INLINE NkHandle pid2handle(pid_t pid) {
    return (NkHandle){(intptr_t)pid};
}

#ifdef __cplusplus
}
#endif

#endif // NTK_POSIX_COMMON_H_
