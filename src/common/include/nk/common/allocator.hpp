#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
#define HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP

#include "nk/common/allocator.h"

template <class T>
T *nk_alloc_t(NkAllocator alloc, size_t n = 1) {
    return (T *)nk_alloc(alloc, n * sizeof(T));
}

template <class T>
T *nk_realloc_t(NkAllocator alloc, size_t n, T *old_mem, size_t old_n) {
    return (T *)nk_realloc(alloc, n * sizeof(T), (void *)old_mem, old_n * sizeof(T));
}

template <class T>
void nk_free_t(NkAllocator alloc, T *ptr, size_t n = 1) {
    nk_free(alloc, ptr, n * sizeof(T));
}

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
