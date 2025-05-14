#include "ntk/thread.h"

#include "common.h"

NkHandle nk_mutex_alloc(void) {
    return native2handle(CreateMutex(NULL, FALSE, NULL));
}

i32 nk_mutex_free(NkHandle h_mutex) {
    nk_assert(!nk_handleIsNull(h_mutex) && "Using uninitialized mutex");
    return CloseHandle(handle2native(h_mutex)) ? 0 : -1;
}

i32 nk_mutex_lock(NkHandle h_mutex) {
    nk_assert(!nk_handleIsNull(h_mutex) && "Using uninitialized mutex");
    return WaitForSingleObject(handle2native(h_mutex), INFINITE) == WAIT_FAILED ? -1 : 0;
}

i32 nk_mutex_unlock(NkHandle h_mutex) {
    nk_assert(!nk_handleIsNull(h_mutex) && "Using uninitialized mutex");
    return ReleaseMutex(handle2native(h_mutex)) ? 0 : -1;
}
