#ifndef NTK_WINDOWS_COMMON_H_
#define NTK_WINDOWS_COMMON_H_

#include "ntk/common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

NK_INLINE HANDLE handle_toNative(NkHandle handle) {
    return (HANDLE)handle.val;
}

NK_INLINE NkHandle handle_fromNative(HANDLE native_handle) {
    return (NkHandle){(intptr_t)native_handle};
}

#ifdef __cplusplus
}
#endif

#endif // NTK_WINDOWS_COMMON_H_
