#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_ALLOCATOR

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkAllocator NkAllocator;

typedef void *(*NkAllocateFunc)(NkAllocator *alloc, size_t size);
typedef void (*NkFreeFunc)(NkAllocator *alloc, void *ptr);

struct NkAllocator {
    NkAllocateFunc allocate;
    NkFreeFunc free;
};

inline void *nk_allocate(NkAllocator *alloc, size_t size) {
    return alloc->allocate(alloc, size);
}

inline void nk_free(NkAllocator *alloc, void *ptr) {
    alloc->free(alloc, ptr);
}

extern NkAllocator *nk_default_allocator;

NkAllocator *nk_create_arena();
void nk_free_arena(NkAllocator *alloc);

typedef struct NkStackAllocator NkStackAllocator;

typedef struct {
    size_t size;
} NkStackAllocatorFrame;

struct NkStackAllocator {
    void *(*allocate)(NkStackAllocator *alloc, size_t size);
    NkStackAllocatorFrame (*pushFrame)(NkStackAllocator *);
    void (*popFrame)(NkStackAllocator *, NkStackAllocatorFrame);
};

inline void *nk_stack_allocate(NkStackAllocator *alloc, size_t size) {
    return alloc->allocate(alloc, size);
}

inline NkStackAllocatorFrame nk_stack_pushFrame(NkStackAllocator *alloc) {
    return alloc->pushFrame(alloc);
}

inline void nk_stack_popFrame(NkStackAllocator *alloc, NkStackAllocatorFrame frame) {
    alloc->popFrame(alloc, frame);
}

NkStackAllocator *nk_create_stack();
void nk_free_stack(NkStackAllocator *alloc);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR