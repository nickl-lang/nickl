#ifndef NTK_OS_MEM_H_
#define NTK_OS_MEM_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

void *nk_mem_reserveAndCommit(usize len);
i32 nk_mem_release(void *addr, usize len);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_MEM_H_
