#ifndef HEADER_GUARD_NK_COMMON_MEM
#define HEADER_GUARD_NK_COMMON_MEM

#include <cstddef>

void *platform_alloc(size_t size);
void platform_free(void *ptr);

#endif // HEADER_GUARD_NK_COMMON_MEM
