#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/sys/mem.h"

namespace {

NK_LOG_USE_SCOPE(arena);

static constexpr size_t FIXED_ARENA_SIZE = 1 << 24; // 16Mib

void *arenaAllocatorProc(void *data, NkAllocatorMode mode, size_t size, void *old_mem, size_t old_size) {
    (void)old_mem;

    auto arena = (NkArena *)data;

#ifdef ENABLE_LOGGING
    switch (mode) {
    case NkAllocator_Alloc:
        NK_LOG_TRC("arena=%p alloc(%" PRIu64 ")", data, size);
        break;
    case NkAllocator_Free:
        NK_LOG_TRC("arena=%p free(%p, %" PRIu64 ")", data, old_mem, old_size);
        break;
    case NkAllocator_Realloc:
        NK_LOG_TRC("arena=%p realloc(%" PRIu64 ", %p, %" PRIu64 ")", data, size, old_mem, old_size);
        break;
    default:
        assert(!"unreachable");
    case NkAllocator_QuerySpaceLeft:
        break;
    }
#endif // ENABLE_LOGGING

    switch (mode) {
    case NkAllocator_Realloc:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        [[fallthrough]];

    case NkAllocator_Alloc:
        return nk_arena_alloc(arena, size);

    case NkAllocator_Free:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        return nullptr;

    case NkAllocator_QuerySpaceLeft:
        *(NkAllocatorSpaceLeftQueryResult *)old_mem = {
            .kind = NkAllocatorSpaceLeft_Limited,
            .bytes_left = (arena->data ? arena->capacity : FIXED_ARENA_SIZE) - arena->size,
        };
        return nullptr;

    default:
        assert(!"unreachable");
        return nullptr;
    }
}

} // namespace

void *nk_arena_alloc(NkArena *arena, size_t size) {
    if (!arena->data) {
        // TODO Fixed sized arena
        arena->data = (uint8_t *)nk_valloc(FIXED_ARENA_SIZE);
        arena->size = 0;
        arena->capacity = FIXED_ARENA_SIZE;
    }

    auto mem = arena->data + arena->size;
    arena->size += size;
    return mem;
}

void nk_arena_pop(NkArena *arena, size_t size) {
    arena->size -= size;
}

void nk_arena_free(NkArena *arena) {
    if (arena->data) {
        nk_vfree(arena->data, FIXED_ARENA_SIZE);
    }
    *arena = {};
}

NkAllocator nk_arena_getAllocator(NkArena *arena) {
    return {
        .data = arena,
        .proc = arenaAllocatorProc,
    };
}
