#ifndef NTK_OS_THREAD_H_
#define NTK_OS_THREAD_H_

#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NkOsHandle nk_mutex_alloc(void);
i32 nk_mutex_free(NkOsHandle mutex);

i32 nk_mutex_lock(NkOsHandle mutex);
i32 nk_mutex_unlock(NkOsHandle mutex);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_THREAD_H_
