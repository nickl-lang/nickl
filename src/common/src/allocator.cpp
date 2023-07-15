#include "nk/common/allocator.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "nk/sys/mem.h"

namespace {

void *defaultAllocatorProc(void * /*data*/, NkAllocatorMode mode, size_t size, void *old_mem, size_t /*old_size*/) {
    switch (mode) {
    case NkAllocator_Alloc:
        return std::malloc(size);

    case NkAllocator_Free:
        std::free(old_mem);
        return nullptr;

    case NkAllocator_Realloc:
        return std::realloc(old_mem, size);

    default:
        assert(!"unreachable");
        return nullptr;
    }
}

static constexpr size_t FIXED_ARENA_SIZE = 1 << 24; // 16Mib

void *arenaAllocatorProc(void *data, NkAllocatorMode mode, size_t size, void *old_mem, size_t old_size) {
    (void)old_mem;

    auto arena = (NkArenaAllocator *)data;

    if (!arena->data) {
        // TODO Fixed sized arena
        arena->data = (uint8_t *)nk_mmap(FIXED_ARENA_SIZE);
        arena->size = 0;
    }

    switch (mode) {
    case NkAllocator_Realloc:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            arena->size -= old_size;
        }
        [[fallthrough]];

    case NkAllocator_Alloc: {
        auto mem = arena->data + arena->size;
        arena->size += size;
        return mem;
    }

    default:
        assert(!"unreachable");

    case NkAllocator_Free:
        return nullptr;
    }
}

} // namespace

NkAllocator nk_default_allocator = {
    .data = nullptr,
    .proc = defaultAllocatorProc,
};

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
