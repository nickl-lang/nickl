#ifndef HEADER_GUARD_NTK_ALLOCATOR
#define HEADER_GUARD_NTK_ALLOCATOR

#include <stddef.h>
#include <stdint.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkAllocator_Alloc,
    NkAllocator_Free,
    NkAllocator_Realloc,
    NkAllocator_QuerySpaceLeft,
} NkAllocatorMode;

typedef void *(*NkAllocateProc)(void *data, NkAllocatorMode mode, usize size, u8 align, void *old_mem, usize old_size);

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
    usize bytes_left;
} NkAllocatorSpaceLeftQueryResult;

NK_INLINE void *nk_allocAligned(NkAllocator alloc, usize size, u8 align) {
    return alloc.proc(alloc.data, NkAllocator_Alloc, size, align, NULL, 0);
}

NK_INLINE void nk_freeAligned(NkAllocator alloc, void *ptr, usize size, u8 align) {
    alloc.proc(alloc.data, NkAllocator_Free, 0, align, ptr, size);
}

NK_INLINE void *nk_reallocAligned(NkAllocator alloc, usize size, u8 align, void *old_mem, usize old_size) {
    return alloc.proc(alloc.data, NkAllocator_Realloc, size, align, old_mem, old_size);
}

NK_INLINE void *nk_alloc(NkAllocator alloc, usize size) {
    return nk_allocAligned(alloc, size, 1);
}

NK_INLINE void nk_free(NkAllocator alloc, void *ptr, usize size) {
    nk_freeAligned(alloc, ptr, size, 1);
}

NK_INLINE void *nk_realloc(NkAllocator alloc, usize size, void *old_mem, usize old_size) {
    return nk_reallocAligned(alloc, size, 1, old_mem, old_size);
}

NK_INLINE void *nk_alloc_querySpaceLeft(NkAllocator alloc, NkAllocatorSpaceLeftQueryResult *result) {
    return alloc.proc(alloc.data, NkAllocator_QuerySpaceLeft, 0, 1, result, 0);
}

extern NkAllocator nk_default_allocator;

typedef struct {
    u8 *data;
    usize size;
    usize capacity;
} NkArena;

NkAllocator nk_arena_getAllocator(NkArena *arena);

void *nk_arena_allocAligned(NkArena *arena, usize size, u8 align);

NK_INLINE void *nk_arena_alloc(NkArena *arena, usize size) {
    return nk_arena_allocAligned(arena, size, 1);
}

void nk_arena_pop(NkArena *arena, usize size);

NK_INLINE void nk_arena_clear(NkArena *arena) {
    nk_arena_pop(arena, arena->size);
}

void nk_arena_free(NkArena *arena);

typedef struct {
    usize size;
} NkArenaFrame;

NK_INLINE NkArenaFrame nk_arena_grab(NkArena *arena) {
    return LITERAL(NkArenaFrame){arena->size};
}

NK_INLINE void nk_arena_popFrame(NkArena *arena, NkArenaFrame frame) {
    nk_arena_pop(arena, arena->size - frame.size);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

template <class T>
T *nk_alloc_t(NkAllocator alloc, usize n = 1) {
    return (T *)nk_allocAligned(alloc, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_realloc_t(NkAllocator alloc, usize n, T *old_mem, usize old_n) {
    return (T *)nk_reallocAligned(alloc, n * sizeof(T), alignof(T), (void *)old_mem, old_n * sizeof(T));
}

template <class T>
void nk_free_t(NkAllocator alloc, T *ptr, usize n = 1) {
    nk_freeAligned(alloc, ptr, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_arena_alloc_t(NkArena *arena, usize n = 1) {
    return (T *)nk_arena_allocAligned(arena, n * sizeof(T), alignof(T));
}

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_ALLOCATOR
