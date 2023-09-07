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
    NkAllocator_QuerySpaceLeft,
} NkAllocatorMode;

typedef void *(*NkAllocateProc)(void *data, NkAllocatorMode mode, size_t size, void *old_mem, size_t old_size);

typedef struct {
    void *data;
    NkAllocateProc proc;
} NkAllocator;

typedef enum {
    NkAllocatorSpaceLeft_Unknown,
    NkAllocatorSpaceLeft_Limited,
} NkAllocatorSpaceLeftQueryResultKind;

typedef struct {
    NkAllocatorSpaceLeftQueryResultKind kind;
    size_t bytes_left;
} NkAllocatorSpaceLeftQueryResult;

inline void *nk_alloc(NkAllocator alloc, size_t size) {
    return alloc.proc(alloc.data, NkAllocator_Alloc, size, NULL, 0);
}

inline void nk_free(NkAllocator alloc, void *ptr, size_t size) {
    alloc.proc(alloc.data, NkAllocator_Free, 0, ptr, size);
}

inline void *nk_realloc(NkAllocator alloc, size_t size, void *old_mem, size_t old_size) {
    return alloc.proc(alloc.data, NkAllocator_Realloc, size, old_mem, old_size);
}

inline void *nk_alloc_querySpaceLeft(NkAllocator alloc, NkAllocatorSpaceLeftQueryResult *result) {
    return alloc.proc(alloc.data, NkAllocator_QuerySpaceLeft, 0, result, 0);
}

extern NkAllocator nk_default_allocator;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} NkArena;

NkAllocator nk_arena_getAllocator(NkArena *arena);

void *nk_arena_alloc(NkArena *arena, size_t size);
void *nk_arena_allocAligned(NkArena *arena, size_t size, uint8_t align);
void *nk_arena_allocAlignedRaw(NkArena *arena, size_t size, uint8_t align);

void nk_arena_pop(NkArena *arena, size_t size);

inline void nk_arena_clear(NkArena *arena) {
    nk_arena_pop(arena, arena->size);
}

void nk_arena_free(NkArena *arena);

typedef struct {
    size_t size;
} NkArenaFrame;

inline NkArenaFrame nk_arena_grab(NkArena *arena) {
    return {arena->size};
}

inline void nk_arena_popFrame(NkArena *arena, NkArenaFrame frame) {
    nk_arena_pop(arena, arena->size - frame.size);
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_ALLOCATOR_H
