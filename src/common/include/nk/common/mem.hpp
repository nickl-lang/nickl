#ifndef HEADER_GUARD_NK_COMMON_MEM
#define HEADER_GUARD_NK_COMMON_MEM

#include <cstddef>

struct Allocator {
    virtual void *alloc(size_t size) = 0;
    virtual void free(void *ptr) = 0;

    void *alloc_aligned(size_t size, size_t align);
    void free_aligned(void *ptr);

    template <class T>
    T *alloc(size_t n = 1) {
        return (T *)alloc_aligned(n * sizeof(T), alignof(T));
    }
};

struct LibcAllocator : Allocator {
    void *alloc(size_t size) override;
    void free(void *ptr) override;

    using Allocator::alloc;
};

struct MemCtx {
    Allocator *def_allocator;
    Allocator *tmp_allocator;
};

extern thread_local MemCtx _mctx;

#endif // HEADER_GUARD_NK_COMMON_MEM
