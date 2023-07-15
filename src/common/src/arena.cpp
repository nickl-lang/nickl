#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "nk/common/allocator.h"
#include "nk/sys/mem.h"

namespace {

void *arenaAllocatorProc(void *data, NkAllocatorMode mode, size_t size, void *old_mem, size_t old_size) {
    (void)old_mem;

    auto arena = (NkArenaAllocator *)data;

    switch (mode) {
    case NkAllocator_Realloc:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        [[fallthrough]];

    case NkAllocator_Alloc: {
        return nk_arena_alloc(arena, size);
    }

    case NkAllocator_Free:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        return nullptr;

    default:
        assert(!"unreachable");
        return nullptr;
    }
}

static constexpr size_t FIXED_ARENA_SIZE = 1 << 24; // 16Mib

} // namespace

void *nk_arena_alloc(NkArenaAllocator *arena, size_t size) {
    if (!arena->data) {
        // TODO Fixed sized arena
        arena->data = (uint8_t *)nk_mmap(FIXED_ARENA_SIZE);
        arena->size = 0;
    }

    auto mem = arena->data + arena->size;
    arena->size += size;
    return mem;
}

void nk_arena_pop(NkArenaAllocator *arena, size_t size) {
    arena->size -= size;
}

void nk_arena_free(NkArenaAllocator *arena) {
    if (arena->data) {
        nk_munmap(arena->data, FIXED_ARENA_SIZE);
    }
    *arena = {};
}

NkAllocator nk_arena_getAllocator(NkArenaAllocator *arena) {
    return {
        .data = arena,
        .proc = arenaAllocatorProc,
    };
}
