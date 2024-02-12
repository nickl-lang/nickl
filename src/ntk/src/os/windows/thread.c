#include "ntk/os/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

NkOsHandle nk_mutex_alloc(void) {
    return (NkOsHandle)CreateMutex(NULL, FALSE, NULL);
}

i32 nk_mutex_free(NkOsHandle mutex) {
    return CloseHandle((HANDLE)mutex) ? 0 : -1;
}

i32 nk_mutex_lock(NkOsHandle mutex) {
    return WaitForSingleObject((HANDLE)mutex, INFINITE) == WAIT_FAILED ? -1 : 0;
}

i32 nk_mutex_unlock(NkOsHandle mutex) {
    return ReleaseMutex((HANDLE)mutex) ? 0 : -1;
}
