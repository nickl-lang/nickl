#include "nk/mem/mem.h"

#include <stdint.h>
#include <stdlib.h>

#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"

LOG_USE_SCOPE(mem);

void *nk_platform_alloc(size_t size) {
    EASY_FUNCTION(::profiler::colors::Red200);
    void *mem = malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem);
    return mem;
}

void nk_platform_free(void const *ptr) {
    EASY_FUNCTION(::profiler::colors::Red100);
    LOG_TRC("free(ptr=%p)", ptr);
    free((void *)ptr);
}

void *nk_platform_alloc_aligned(size_t size, size_t align) {
    if (size) {
        size_t const extra = (align - 1) + sizeof(void *);
        void *mem = nk_platform_alloc(size + extra);
        void **ptr_array = (void **)(((uintptr_t)mem + extra) & ~(align - 1));
        ptr_array[-1] = mem;
        return ptr_array;
    } else {
        return NULL;
    }
}

void nk_platform_free_aligned(void const *ptr) {
    if (ptr) {
        nk_platform_free(((void const *const *)ptr)[-1]);
    }
}
