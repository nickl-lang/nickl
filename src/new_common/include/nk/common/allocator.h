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

static inline void *nk_allocate(NkAllocator *alloc, size_t size) {
    return alloc->allocate(alloc, size);
}

static inline void nk_free(NkAllocator *alloc, void *ptr) {
    alloc->free(alloc, ptr);
}

extern NkAllocator *nk_default_allocator;

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR
