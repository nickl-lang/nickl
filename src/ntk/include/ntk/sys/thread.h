#ifndef HEADER_GUARD_NTK_SYS_THREAD
#define HEADER_GUARD_NTK_SYS_THREAD

#include <stdint.h>

#ifdef _WIN32
typedef void *nk_mutex_t;
#else // _WIN32
#include <pthread.h>
typedef pthread_mutex_t nk_mutex_t;
#endif // _WIN32

#ifdef __cplusplus
extern "C" {
#endif

int nk_mutex_init(nk_mutex_t *mutex);
int nk_mutex_lock(nk_mutex_t *mutex);
int nk_mutex_unlock(nk_mutex_t *mutex);

void nk_usleep(uint64_t usec);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_THREAD
