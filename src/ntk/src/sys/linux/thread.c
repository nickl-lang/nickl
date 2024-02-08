#include "ntk/sys/thread.h"

#include <pthread.h>
#include <unistd.h>

int nk_mutex_init(nk_mutex_t *mutex) {
    return pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
}

int nk_mutex_lock(nk_mutex_t *mutex) {
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

int nk_mutex_unlock(nk_mutex_t *mutex) {
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

void nk_usleep(uint64_t usec) {
    usleep(usec);
}
