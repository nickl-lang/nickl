#include "nk/common/mem.h"

#include <stdint.h>
#include <stdlib.h>

#include "nk/common/logger.h"

LOG_USE_SCOPE(mem);

void *platform_alloc(size_t size) {
    void *mem = malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem);
    return mem;
}

void platform_free(void const *ptr) {
    LOG_TRC("free(ptr=%p)", ptr);
    free((void *)ptr);
}

void *platform_alloc_aligned(size_t size, size_t align) {
    if (size) {
        size_t const extra = (align - 1) + sizeof(void *);
        void *mem = platform_alloc(size + extra);
        void **ptr_array = (void **)(((uintptr_t)mem + extra) & ~(align - 1));
        ptr_array[-1] = mem;
        return ptr_array;
    } else {
        return NULL;
    }
}

void platform_free_aligned(void const *ptr) {
    if (ptr) {
        platform_free(((void const *const *)ptr)[-1]);
    }
}
