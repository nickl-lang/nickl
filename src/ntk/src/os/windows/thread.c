#include "ntk/thread.h"

#include "common.h"

NkHandle nk_mutex_alloc(void) {
    return handle_fromNative(CreateMutex(NULL, FALSE, NULL));
}

i32 nk_mutex_free(NkHandle h_mutex) {
    nk_assert(!nk_handleIsZero(h_mutex) && "Using uninitialized mutex");
    return CloseHandle(handle_toNative(h_mutex)) ? 0 : -1;
}

i32 nk_mutex_lock(NkHandle h_mutex) {
    nk_assert(!nk_handleIsZero(h_mutex) && "Using uninitialized mutex");
    return WaitForSingleObject(handle_toNative(h_mutex), INFINITE) == WAIT_FAILED ? -1 : 0;
}

i32 nk_mutex_unlock(NkHandle h_mutex) {
    nk_assert(!nk_handleIsZero(h_mutex) && "Using uninitialized mutex");
    return ReleaseMutex(handle_toNative(h_mutex)) ? 0 : -1;
}
