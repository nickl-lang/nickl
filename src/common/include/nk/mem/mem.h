#ifndef HEADER_GUARD_NK_COMMON_MEM
#define HEADER_GUARD_NK_COMMON_MEM

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *nk_platform_alloc(size_t size);
void nk_platform_free(void const *ptr);

void *nk_platform_alloc_aligned(size_t size, size_t align);
void nk_platform_free_aligned(void const *ptr);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_MEM
