#include "nk/common/allocator.h"

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <new>

namespace {

void *defaultAllocate(NkAllocator *, size_t size) {
    return std::malloc(size);
}

void defaultFree(NkAllocator *, void *ptr) {
    std::free(ptr);
}

struct ArenaAllocator {
    NkAllocator base;
    uint8_t *data;
    size_t size;
};

NkAllocator s_default_allocator = {
    .allocate = defaultAllocate,
    .free = defaultFree,
};

void *arenaAllocate(NkAllocator *alloc, size_t size) {
    auto arena = (ArenaAllocator *)alloc;
    auto mem = arena->data + arena->size;
    arena->size += size;
    return mem;
}

void arenaFree(NkAllocator *, void *) {
}

} // namespace

NkAllocator *nk_default_allocator = &s_default_allocator;

NkAllocator *nk_create_arena() {
    auto arena = new (nk_allocate(nk_default_allocator, sizeof(ArenaAllocator))) ArenaAllocator{
        .base{
            .allocate = arenaAllocate,
            .free = arenaFree,
        },
        .data = (uint8_t *)nk_allocate(nk_default_allocator, 1000000), //@TODO Fixed sized arena
        .size = 0,
    };
    return (NkAllocator *)arena;
}

void nk_free_arena(NkAllocator *alloc) {
    auto arena = (ArenaAllocator *)alloc;
    nk_free(nk_default_allocator, arena->data);
    nk_free(nk_default_allocator, arena);
}
