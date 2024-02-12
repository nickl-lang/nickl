#include "ntk/os/thread.h"

#include <pthread.h>

i32 nk_mutex_init(nk_mutex_t *mutex) {
    return pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
}

i32 nk_mutex_lock(nk_mutex_t *mutex) {
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

i32 nk_mutex_unlock(nk_mutex_t *mutex) {
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}
