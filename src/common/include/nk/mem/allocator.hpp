#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_ALLOCATOR

#include <cstddef>

#include "nk/ds/slice.hpp"

namespace nk {

struct Allocator {
    template <class T>
    Slice<T> alloc(size_t n = 1) {
        return {(T *)alloc_aligned(n * sizeof(T), alignof(T)), n};
    }

    virtual void *alloc_aligned(size_t size, size_t align) = 0;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR
