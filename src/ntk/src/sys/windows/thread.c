#include "ntk/sys/thread.h"

#include <unistd.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

i32 nk_mutex_init(nk_mutex_t *mutex) {
    HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex) {
        *mutex = hMutex;
        return 0;
    } else {
        return -1;
    };
}

i32 nk_mutex_lock(nk_mutex_t *mutex) {
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED ? -1 : 0;
}

i32 nk_mutex_unlock(nk_mutex_t *mutex) {
    return ReleaseMutex(*mutex) ? 0 : -1;
}

void nk_usleep(u64 usec) {
    usleep(usec);
}
