#ifndef NTK_OS_LINUX_COMMON_H_
#define NTK_OS_LINUX_COMMON_H_

#include <unistd.h>

#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_INLINE void *handle_toNative(NkOsHandle handle) {
    return (void *)handle.val;
}

NK_INLINE NkOsHandle handle_fromNative(void *native_handle) {
    return (NkOsHandle){(intptr_t)native_handle};
}

NK_INLINE i32 handle_toFd(NkOsHandle handle) {
    return (i32)handle.val - 1;
}

NK_INLINE NkOsHandle handle_fromFd(i32 fd) {
    return (NkOsHandle){(intptr_t)(fd + 1)};
}

NK_INLINE pid_t handle_toPid(NkOsHandle handle) {
    return (pid_t)handle.val;
}

NK_INLINE NkOsHandle handle_fromPid(pid_t pid) {
    return (NkOsHandle){(intptr_t)pid};
}

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_LINUX_COMMON_H_
