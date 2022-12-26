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

NkAllocator s_default_allocator = {
    .allocate = defaultAllocate,
    .free = defaultFree,
};

struct ArenaAllocator {
    NkAllocator base;
    uint8_t *data;
    size_t size;
};

void *arenaAllocate(NkAllocator *alloc, size_t size) {
    auto arena = (ArenaAllocator *)alloc;
    auto mem = arena->data + arena->size;
    arena->size += size;
    return mem;
}

void arenaFree(NkAllocator *, void *) {
}

struct StackAllocator {
    NkStackAllocator base;
    uint8_t *data;
    size_t size;
};

void *stackAllocate(NkStackAllocator *alloc, size_t size) {
    auto stack = (StackAllocator *)alloc;
    auto mem = stack->data + stack->size;
    stack->size += size;
    return mem;
}

NkStackAllocatorFrame stackGetFrame(NkStackAllocator *alloc) {
    auto stack = (StackAllocator *)alloc;
    return {stack->size};
}

void stackPopFrame(NkStackAllocator *alloc, NkStackAllocatorFrame frame) {
    auto stack = (StackAllocator *)alloc;
    stack->size = frame.size;
}

} // namespace

NkAllocator *nk_default_allocator = &s_default_allocator;

NkAllocator *nk_create_arena() {
    return (NkAllocator *)new (
        nk_allocate(nk_default_allocator, sizeof(ArenaAllocator))) ArenaAllocator{
        .base{
            .allocate = arenaAllocate,
            .free = arenaFree,
        },
        .data = (uint8_t *)nk_allocate(nk_default_allocator, 10000000), //@TODO Fixed sized arena
        .size = 0,
    };
}

void nk_free_arena(NkAllocator *alloc) {
    auto arena = (ArenaAllocator *)alloc;
    nk_free(nk_default_allocator, arena->data);
    nk_free(nk_default_allocator, arena);
}

NkStackAllocator *nk_create_stack() {
    return (NkStackAllocator *)new (
        nk_allocate(nk_default_allocator, sizeof(StackAllocator))) StackAllocator{
        .base{
            .allocate = stackAllocate,
            .getFrame = stackGetFrame,
            .popFrame = stackPopFrame,
        },
        .data = (uint8_t *)nk_allocate(nk_default_allocator, 10000000), //@TODO Fixed sized stack
        .size = 0,
    };
}

void nk_free_stack(NkStackAllocator *alloc) {
    auto stack = (StackAllocator *)alloc;
    nk_free(nk_default_allocator, stack->data);
    nk_free(nk_default_allocator, stack);
}
