#include "nk/common/allocator.h"

#include <stdlib.h>

void *nk_allocate(NkAllocator *alloc, size_t size);
void nk_free(NkAllocator *alloc, void *ptr);

static void *defaultAllocate(NkAllocator *alloc, size_t size) {
    (void)alloc;
    return malloc(size);
}

static void defaultFree(NkAllocator *alloc, void *ptr) {
    (void)alloc;
    free(ptr);
}

static NkAllocator s_default_allocator = {
    .allocate = defaultAllocate,
    .free = defaultFree,
};

NkAllocator *nk_default_allocator = &s_default_allocator;
