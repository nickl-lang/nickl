#ifndef HEADER_GUARD_NK_SYS_MEM
#define HEADER_GUARD_NK_SYS_MEM

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *nk_valloc(size_t len);
int nk_vfree(void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_MEM
