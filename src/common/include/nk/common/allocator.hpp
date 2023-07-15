#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
#define HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP

#include "nk/common/allocator.h"

template <class T>
T *nk_alloc(NkAllocator alloc, size_t n = 1) {
    return (T *)nk_alloc(alloc, n * sizeof(T));
}

template <class T>
T *nk_realloc(NkAllocator alloc, size_t n, T *old_mem, size_t old_n) {
    return (T *)nk_realloc(alloc, n * sizeof(T), old_mem, old_n * sizeof(T));
}

template <class T>
void nk_free(NkAllocator alloc, T *ptr) {
    nk_free(alloc, (void *)ptr);
}

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
