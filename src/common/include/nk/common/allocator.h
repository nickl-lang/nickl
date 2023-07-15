#ifndef HEADER_GUARD_NK_COMMON_ALLOCATOR_H
#define HEADER_GUARD_NK_COMMON_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkAllocator_Alloc,
    NkAllocator_Free,
    NkAllocator_Realloc,
} NkAllocatorMode;

typedef void *(*NkAllocateProc)(void *data, NkAllocatorMode mode, size_t size, void *old_mem, size_t old_size);

typedef struct {
    void *data;
    NkAllocateProc proc;
} NkAllocator;

inline void *nk_alloc(NkAllocator alloc, size_t size) {
    return alloc.proc(alloc.data, NkAllocator_Alloc, size, NULL, 0);
}

inline void nk_free(NkAllocator alloc, void *ptr) {
    // TODO Accept old_size for nk_free
    alloc.proc(alloc.data, NkAllocator_Free, 0, ptr, 0);
}

inline void *nk_realloc(NkAllocator alloc, size_t size, void *old_mem, size_t old_size) {
    return alloc.proc(alloc.data, NkAllocator_Realloc, size, old_mem, old_size);
}

extern NkAllocator nk_default_allocator;

typedef struct {
    uint8_t *data;
    size_t size;
} NkArenaAllocator;

NkAllocator nk_arena_getAllocator(NkArenaAllocator *arena);

inline void *nk_arena_alloc(NkArenaAllocator *arena, size_t size) {
    return nk_alloc(nk_arena_getAllocator(arena), size);
}

void nk_arena_free(NkArenaAllocator *arena);

typedef struct {
    size_t size;
} NkArenaAllocatorFrame;

inline NkArenaAllocatorFrame nk_arena_grab(NkArenaAllocator *arena) {
    return {arena->size};
}

inline void nk_arena_pop(NkArenaAllocator *arena, NkArenaAllocatorFrame frame) {
    arena->size = frame.size;
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR_H
