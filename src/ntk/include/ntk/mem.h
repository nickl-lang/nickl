#ifndef NTK_MEM_H_
#define NTK_MEM_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT void *nk_mem_reserveAndCommit(usize len);
NK_EXPORT i32 nk_mem_release(void *addr, usize len);

#ifdef __cplusplus
}
#endif

#endif // NTK_MEM_H_
