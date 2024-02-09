#ifndef HEADER_GUARD_NTK_SYS_MEM
#define HEADER_GUARD_NTK_SYS_MEM

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

void *nk_valloc(usize len);
i32 nk_vfree(void *addr, usize len);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_MEM
