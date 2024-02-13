#ifndef NTK_OS_WINDOWS_COMMON_H_
#define NTK_OS_WINDOWS_COMMON_H_

#include "ntk/os/common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

NK_INLINE HANDLE handle_toNative(NkOsHandle handle) {
    return (HANDLE)handle.val;
}

NK_INLINE NkOsHandle handle_fromNative(HANDLE native_handle) {
    return (NkOsHandle){(intptr_t)native_handle};
}

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_WINDOWS_COMMON_H_
