#ifndef HEADER_GUARD_NK_COMMON_MEM
#define HEADER_GUARD_NK_COMMON_MEM

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *platform_alloc(size_t size);
void platform_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_MEM
