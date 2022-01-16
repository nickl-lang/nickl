#ifndef HEADER_GUARD_MEM
#define HEADER_GUARD_MEM

#include <cstddef>

void *lang_malloc(size_t size);
void *lang_realloc(void *ptr, size_t new_size);
void lang_free(void *ptr);

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

#endif // HEADER_GUARD_MEM
