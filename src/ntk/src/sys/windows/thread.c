#include "ntk/sys/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int nk_mutex_init(nk_mutex_t *mutex) {
    HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex) {
        *mutex = hMutex;
        return 0;
    } else {
        return -1;
    };
}

int nk_mutex_lock(nk_mutex_t *mutex) {
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED ? -1 : 0;
}

int nk_mutex_unlock(nk_mutex_t *mutex) {
    return ReleaseMutex(*mutex) ? 0 : -1;
}
