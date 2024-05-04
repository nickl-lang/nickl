#include "ntk/arena.h"

#include "ntk/log.h"
#include "ntk/os/mem.h"
#include "ntk/utils.h"

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#define RED_ZONE_SIZE 32
#else
#define RED_ZONE_SIZE 0
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

NK_LOG_USE_SCOPE(arena);

// TODO Hardcoded arena size
#define FIXED_ARENA_SIZE ((usize)(1 << 24)) // 16 Mib

static void *allocAlignedRaw(NkArena *arena, usize size, u8 align, bool pad) {
    nk_assert(align && nk_isZeroOrPowerOf2(align) && "invalid alignment");

    if (!arena->data) {
        NK_LOG_TRC("arena=%p valloc(%zu)", (void *)arena, FIXED_ARENA_SIZE);
        arena->data = (u8 *)nk_mem_reserveAndCommit(FIXED_ARENA_SIZE);
        ASAN_POISON_MEMORY_REGION(arena->data, FIXED_ARENA_SIZE);
        arena->size = 0;
        arena->capacity = FIXED_ARENA_SIZE;
    }

    u8 *mem = arena->data + arena->size;
    mem += pad * ((align - ((usize)mem & (align - 1))) & (align - 1));

    if (mem + size > arena->data + arena->capacity) {
        NK_LOG_ERR("Out of memory");
        nk_trap();
    } else {
        mem += pad * nk_minu(RED_ZONE_SIZE, arena->data + arena->capacity - mem - size);
    }

    ASAN_UNPOISON_MEMORY_REGION(mem, size);
    arena->size += mem - (arena->data + arena->size) + size;
    return mem;
}

static void *arenaAllocatorProc(void *data, NkAllocatorMode mode, usize size, u8 align, void *old_mem, usize old_size) {
    (void)old_mem;

    NkArena *arena = data;

    switch (mode) {
        case NkAllocatorMode_Alloc: {
            void *ret = allocAlignedRaw(arena, size, align, true);
            NK_LOG_TRC("arena=%p alloc(%zu, %hhu) -> %p", data, size, align, ret);
            return ret;
        }

        case NkAllocatorMode_Free:
            NK_LOG_TRC("arena=%p free(%p, %zu, %hhu)", data, old_mem, old_size, align);

            nk_assert(arena->data + arena->size >= (u8 *)old_mem + old_size && "invalid allocation");
            nk_assert(((usize)old_mem & (align - 1)) == 0 && "invalid alignment");

            if (arena->data + arena->size == (u8 *)old_mem + old_size) {
                nk_arena_pop(arena, old_size);
            }
            return NULL;

        case NkAllocatorMode_Realloc: {
            nk_assert(arena->data + arena->size >= (u8 *)old_mem + old_size && "invalid allocation");
            nk_assert(((usize)old_mem & (align - 1)) == 0 && "invalid alignment");

            void *ret = NULL;

            if (arena->data + arena->size == (u8 *)old_mem + old_size) {
                nk_arena_pop(arena, old_size);
                ret = allocAlignedRaw(arena, size, align, false);
            } else {
                ret = allocAlignedRaw(arena, size, align, true);
                memcpy(ret, old_mem, old_size);
            }

            NK_LOG_TRC("arena=%p realloc(%zu, %hhu, %p, %zu) -> %p", data, size, align, old_mem, old_size, ret);

            return ret;
        }

        case NkAllocatorMode_QuerySpaceLeft:
            *(NkAllocatorSpaceLeftQueryResult *)old_mem = (NkAllocatorSpaceLeftQueryResult){
                .kind = NkAllocatorSpaceLeftQueryResultKind_Limited,
                .bytes_left = (arena->data ? arena->capacity : FIXED_ARENA_SIZE) - arena->size,
            };
            return NULL;

        default:
            nk_assert(!"unreachable");
            return NULL;
    }
}

NkAllocator nk_arena_getAllocator(NkArena *arena) {
    return (NkAllocator){
        .data = arena,
        .proc = arenaAllocatorProc,
    };
}

void *nk_arena_allocAligned(NkArena *arena, usize size, u8 align) {
    return allocAlignedRaw(arena, size, align, true);
}

void nk_arena_pop(NkArena *arena, usize size) {
    nk_assert(arena->size >= size && "trying to pop more bytes that available");
    arena->size -= size;
    ASAN_POISON_MEMORY_REGION(arena->data + arena->size, size);
}

void nk_arena_free(NkArena *arena) {
    if (arena->data) {
        NK_LOG_TRC("arena=%p vfree(%p, %zu)", (void *)arena, (void *)arena->data, FIXED_ARENA_SIZE);
        ASAN_UNPOISON_MEMORY_REGION(arena->data, FIXED_ARENA_SIZE);
        nk_mem_release(arena->data, FIXED_ARENA_SIZE);
    }
    *arena = (NkArena){0};
}
