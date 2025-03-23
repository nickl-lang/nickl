#include "ntk/os/thread.h"

#include <pthread.h>

#include "common.h"
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
    return handle_fromNative(mutex);
}

i32 nk_mutex_free(NkOsHandle h_mutex) {
    nk_assert(!nkos_handleIsZero(h_mutex) && "Using uninitialized mutex");

    pthread_mutex_t *mutex = handle_toNative(h_mutex);
    i32 res = pthread_mutex_destroy(mutex);

    pthread_mutex_lock(&g_mutex);
    MutexPool_release(&g_mutex_pool, mutex);
    pthread_mutex_unlock(&g_mutex);

    return res;
}

i32 nk_mutex_lock(NkOsHandle h_mutex) {
    nk_assert(!nkos_handleIsZero(h_mutex) && "Using uninitialized mutex");
    return pthread_mutex_lock(handle_toNative(h_mutex));
}

i32 nk_mutex_unlock(NkOsHandle h_mutex) {
    nk_assert(!nkos_handleIsZero(h_mutex) && "Using uninitialized mutex");
    return pthread_mutex_unlock(handle_toNative(h_mutex));
}
