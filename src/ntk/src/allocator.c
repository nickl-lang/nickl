#include "ntk/allocator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ntk/logger.h"

NK_LOG_USE_SCOPE(mem);

static void *defaultAllocatorProc(
    void *data,
    NkAllocatorMode mode,
    usize size,
    u8 align,
    void *old_mem,
    usize old_size) {
    (void)data;
    (void)align;
    (void)old_size;

    switch (mode) {
    case NkAllocator_Alloc: {
        NK_LOG_TRC("malloc(%" PRIu64 ")", size);
        void *data = malloc(size);
        if (!data) {
            NK_LOG_ERR("Out of memory");
            abort();
        }
        return data;
    }

    case NkAllocator_Free: {
        NK_LOG_TRC("free(%p)", old_mem);
        free(old_mem);
        return NULL;
    }

    case NkAllocator_Realloc: {
        NK_LOG_TRC("realloc(%" PRIu64 ", %p)", size, old_mem);
        void *data = realloc(old_mem, size);
        if (!data) {
            NK_LOG_ERR("Out of memory");
            abort();
        }
        return data;
    }

    case NkAllocator_QuerySpaceLeft: {
        *(NkAllocatorSpaceLeftQueryResult *)old_mem = (NkAllocatorSpaceLeftQueryResult){
            .kind = NkAllocatorSpaceLeft_Unknown,
            .bytes_left = 0,
        };
        return NULL;
    }

    default:
        assert(!"unreachable");
        return NULL;
    }
}

NkAllocator nk_default_allocator = {
    .data = NULL,
    .proc = defaultAllocatorProc,
};

extern inline void *nk_allocAligned(NkAllocator alloc, usize size, u8 align);
extern inline void nk_freeAligned(NkAllocator alloc, void *ptr, usize size, u8 align);
extern inline void *nk_reallocAligned(NkAllocator alloc, usize size, u8 align, void *old_mem, usize old_size);
extern inline void *nk_alloc(NkAllocator alloc, usize size);
extern inline void nk_free(NkAllocator alloc, void *ptr, usize size);
extern inline void *nk_realloc(NkAllocator alloc, usize size, void *old_mem, usize old_size);
extern inline void *nk_alloc_querySpaceLeft(NkAllocator alloc, NkAllocatorSpaceLeftQueryResult *result);
extern inline void *nk_arena_alloc(NkArena *arena, usize size);
extern inline void nk_arena_clear(NkArena *arena);
extern inline NkArenaFrame nk_arena_grab(NkArena *arena);
extern inline void nk_arena_popFrame(NkArena *arena, NkArenaFrame frame);
