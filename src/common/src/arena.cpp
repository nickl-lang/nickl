#include <cassert>
#include <cstdint>
#include <cstdlib>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/sys/mem.h"

namespace {

NK_LOG_USE_SCOPE(arena);

static constexpr size_t FIXED_ARENA_SIZE = 1 << 24; // 16 Mib

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
    case NkAllocator_Alloc:
        return nk_arena_alloc(arena, size);

    case NkAllocator_Free:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        return nullptr;

    case NkAllocator_Realloc:
        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
            return nk_arena_alloc(arena, size);
        } else {
            auto new_mem = nk_arena_alloc(arena, size);
            memcpy(new_mem, old_mem, old_size);
            return new_mem;
        }

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
    return nk_arena_allocAlignedRaw(arena, size, 1);
}

void *nk_arena_allocAligned(NkArena *arena, size_t size, uint8_t align) {
    NK_LOG_ERR("TODO nk_arena_allocAligned not implemented");
    std::abort();
}

void *nk_arena_allocAlignedRaw(NkArena *arena, size_t size, uint8_t align) {
    assert(align && isZeroOrPowerOf2(align) && "invalid alignment");

    if (!arena->data) {
        // TODO Fixed sized arena
        NK_LOG_TRC("arena=%p valloc(%" PRIu64 ")", (void *)arena, FIXED_ARENA_SIZE);
        arena->data = (uint8_t *)nk_valloc(FIXED_ARENA_SIZE);
        // TODO Add redzones
        ASAN_POISON_MEMORY_REGION(arena->data, FIXED_ARENA_SIZE);
        arena->size = 0;
        arena->capacity = FIXED_ARENA_SIZE;
    }

    auto const mem = arena->data + arena->size;
    auto const padding = (align - ((size_t)mem & (align - 1))) & (align - 1);

    if (size + padding > arena->capacity - arena->size) {
        NK_LOG_ERR("Out of memory");
        std::abort();
    }

    ASAN_UNPOISON_MEMORY_REGION(mem + padding, size);
    arena->size += size + padding;
    return mem + padding;
}

void nk_arena_pop(NkArena *arena, size_t size) {
    arena->size -= size;
    ASAN_POISON_MEMORY_REGION(arena->data + arena->size, size);
}

void nk_arena_free(NkArena *arena) {
    if (arena->data) {
        NK_LOG_TRC("arena=%p vfree(%p, %" PRIu64 ")", (void *)arena, (void *)arena->data, FIXED_ARENA_SIZE);
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
