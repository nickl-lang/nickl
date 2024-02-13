#include "ntk/os/thread.h"

#include "common.h"

NkOsHandle nk_mutex_alloc(void) {
    return handle_fromNative(CreateMutex(NULL, FALSE, NULL));
}

i32 nk_mutex_free(NkOsHandle h_mutex) {
    return CloseHandle(handle_toNative(h_mutex)) ? 0 : -1;
}

i32 nk_mutex_lock(NkOsHandle h_mutex) {
    return WaitForSingleObject(handle_toNative(h_mutex), INFINITE) == WAIT_FAILED ? -1 : 0;
}

i32 nk_mutex_unlock(NkOsHandle h_mutex) {
    return ReleaseMutex(handle_toNative(h_mutex)) ? 0 : -1;
}
