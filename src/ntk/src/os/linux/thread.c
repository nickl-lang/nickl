#include "ntk/os/thread.h"

#include <pthread.h>

#include "ntk/arena.h"
#include "ntk/pool.h"

NK_POOL_DEFINE(MutexPool, pthread_mutex_t);

static MutexPool g_mutex_pool;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

NkOsHandle nk_mutex_alloc(void) {
    pthread_mutex_lock(&g_mutex);
    pthread_mutex_t *mutex = MutexPool_alloc(&g_mutex_pool);
    pthread_mutex_unlock(&g_mutex);

    pthread_mutex_init(mutex, NULL);
    return (NkOsHandle)mutex;
}

i32 nk_mutex_free(NkOsHandle handle) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)handle;
    i32 res = pthread_mutex_destroy(mutex);

    pthread_mutex_lock(&g_mutex);
    MutexPool_release(&g_mutex_pool, mutex);
    pthread_mutex_unlock(&g_mutex);

    return res;
}

i32 nk_mutex_lock(NkOsHandle mutex) {
    return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

i32 nk_mutex_unlock(NkOsHandle mutex) {
    return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}
