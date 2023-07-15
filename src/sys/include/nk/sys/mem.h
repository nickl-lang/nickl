#ifndef HEADER_GUARD_NK_SYS_MEM
#define HEADER_GUARD_NK_SYS_MEM

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *nk_mmap(size_t len);
void nk_munmap(void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_MEM
