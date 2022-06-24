#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_ALLOCATOR

#include <cstddef>

struct Allocator {
    template <class T>
    T *alloc(size_t n = 1) {
        return (T *)alloc_aligned(n * sizeof(T), alignof(T));
    }

protected:
    virtual void *alloc_aligned(size_t size, size_t align) = 0;
};

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR
