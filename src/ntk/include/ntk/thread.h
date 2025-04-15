#ifndef NTK_THREAD_H_
#define NTK_THREAD_H_

#include "ntk/common.h"
#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT NkHandle nk_mutex_alloc(void);
NK_EXPORT i32 nk_mutex_free(NkHandle mutex);

NK_EXPORT i32 nk_mutex_lock(NkHandle mutex);
NK_EXPORT i32 nk_mutex_unlock(NkHandle mutex);

#define NK_MUTEX_GUARD_SCOPE(mtx) NK_DEFER_LOOP(nk_mutex_lock(mtx), nk_mutex_unlock(mtx))

#ifdef __cplusplus
}
#endif

#endif // NTK_THREAD_H_
