#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
#define HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP

#include "nk/common/allocator.h"

template <class T>
T *nk_alloc_t(NkAllocator alloc, size_t n = 1) {
    return (T *)nk_allocAligned(alloc, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_realloc_t(NkAllocator alloc, size_t n, T *old_mem, size_t old_n) {
    return (T *)nk_reallocAligned(alloc, n * sizeof(T), alignof(T), (void *)old_mem, old_n * sizeof(T));
}

template <class T>
void nk_free_t(NkAllocator alloc, T *ptr, size_t n = 1) {
    nk_freeAligned(alloc, ptr, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_arena_alloc_t(NkArena *arena, size_t n = 1) {
    return (T *)nk_arena_allocAligned(arena, n * sizeof(T), alignof(T));
}

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR_HPP
