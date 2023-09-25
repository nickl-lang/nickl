#include <cassert>
#include <cstdint>
#include <cstdlib>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
static constexpr size_t RED_ZONE_SIZE = 16 * 2;
#else
static constexpr size_t RED_ZONE_SIZE = 0;
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

// TODO Hardcoded arena size
static constexpr size_t FIXED_ARENA_SIZE = 1 << 24; // 16 Mib

void *nk_arena_allocAlignedRaw(NkArena *arena, size_t size, uint8_t align, bool pad = true) {
    assert(align && isZeroOrPowerOf2(align) && "invalid alignment");

    if (!arena->data) {
        NK_LOG_TRC("arena=%p valloc(%" PRIu64 ")", (void *)arena, FIXED_ARENA_SIZE);
        arena->data = (uint8_t *)nk_valloc(FIXED_ARENA_SIZE);
        ASAN_POISON_MEMORY_REGION(arena->data, FIXED_ARENA_SIZE);
        arena->size = 0;
        arena->capacity = FIXED_ARENA_SIZE;
    }

    auto mem = arena->data + arena->size;
    mem += pad * ((align - ((size_t)mem & (align - 1))) & (align - 1));

    if (mem + size > arena->data + arena->capacity) {
        NK_LOG_ERR("Out of memory");
        std::abort();
    } else {
        mem += pad * minu(RED_ZONE_SIZE, arena->data + arena->capacity - mem - size);
    }

    ASAN_UNPOISON_MEMORY_REGION(mem, size);
    arena->size += mem - (arena->data + arena->size) + size;
    return mem;
}

void *arenaAllocatorProc(void *data, NkAllocatorMode mode, size_t size, uint8_t align, void *old_mem, size_t old_size) {
    (void)old_mem;

    auto arena = (NkArena *)data;

#ifdef ENABLE_LOGGING
    switch (mode) {
    case NkAllocator_Alloc:
        NK_LOG_TRC("arena=%p alloc(%" PRIu64 ", %" PRIu8 ")", data, size, align);
        break;
    case NkAllocator_Free:
        NK_LOG_TRC("arena=%p free(%p, %" PRIu64 ", %" PRIu8 ")", data, old_mem, old_size, align);
        break;
    case NkAllocator_Realloc:
        NK_LOG_TRC("arena=%p realloc(%" PRIu64 ", %" PRIu8 ", %p, %" PRIu64 ")", data, size, align, old_mem, old_size);
        break;
    default:
        assert(!"unreachable");
    case NkAllocator_QuerySpaceLeft:
        break;
    }
#endif // ENABLE_LOGGING

    switch (mode) {
    case NkAllocator_Alloc:
        return nk_arena_allocAlignedRaw(arena, size, align);

    case NkAllocator_Free:
        assert(arena->data + arena->size >= (uint8_t *)old_mem + old_size && "invalid allocation");
        assert(((size_t)old_mem & (align - 1)) == 0 && "invalid alignment");

        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
        }
        return nullptr;

    case NkAllocator_Realloc:
        assert(arena->data + arena->size >= (uint8_t *)old_mem + old_size && "invalid allocation");
        assert(((size_t)old_mem & (align - 1)) == 0 && "invalid alignment");

        if (arena->data + arena->size == (uint8_t *)old_mem + old_size) {
            nk_arena_pop(arena, old_size);
            return nk_arena_allocAlignedRaw(arena, size, align, false);
        } else {
            auto new_mem = nk_arena_allocAlignedRaw(arena, size, align);
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

NkAllocator nk_arena_getAllocator(NkArena *arena) {
    return {
        .data = arena,
        .proc = arenaAllocatorProc,
    };
}

void *nk_arena_allocAligned(NkArena *arena, size_t size, uint8_t align) {
    return nk_arena_allocAlignedRaw(arena, size, align);
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
