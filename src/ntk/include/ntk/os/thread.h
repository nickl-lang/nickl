#ifndef NTK_OS_THREAD_H_
#define NTK_OS_THREAD_H_

#include "ntk/os/common.h"

// TODO Unify nk_mutex_t
#ifdef _WIN32
typedef NkOsHandle nk_mutex_t;
#else // _WIN32
#include <pthread.h>
typedef pthread_mutex_t nk_mutex_t;
#endif // _WIN32

#ifdef __cplusplus
extern "C" {
#endif

i32 nk_mutex_init(nk_mutex_t *mutex);
i32 nk_mutex_lock(nk_mutex_t *mutex);
i32 nk_mutex_unlock(nk_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_THREAD_H_
