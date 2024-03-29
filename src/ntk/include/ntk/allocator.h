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

typedef void *(
    *NkAllocateProc)(void *data, NkAllocatorMode mode, size_t size, uint8_t align, void *old_mem, size_t old_size);

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

NK_INLINE void *nk_allocAligned(NkAllocator alloc, size_t size, uint8_t align) {
    return alloc.proc(alloc.data, NkAllocator_Alloc, size, align, NULL, 0);
}

NK_INLINE void nk_freeAligned(NkAllocator alloc, void *ptr, size_t size, uint8_t align) {
    alloc.proc(alloc.data, NkAllocator_Free, 0, align, ptr, size);
}

NK_INLINE void *nk_reallocAligned(NkAllocator alloc, size_t size, uint8_t align, void *old_mem, size_t old_size) {
    return alloc.proc(alloc.data, NkAllocator_Realloc, size, align, old_mem, old_size);
}

NK_INLINE void *nk_alloc(NkAllocator alloc, size_t size) {
    return nk_allocAligned(alloc, size, 1);
}

NK_INLINE void nk_free(NkAllocator alloc, void *ptr, size_t size) {
    nk_freeAligned(alloc, ptr, size, 1);
}

NK_INLINE void *nk_realloc(NkAllocator alloc, size_t size, void *old_mem, size_t old_size) {
    return nk_reallocAligned(alloc, size, 1, old_mem, old_size);
}

NK_INLINE void *nk_alloc_querySpaceLeft(NkAllocator alloc, NkAllocatorSpaceLeftQueryResult *result) {
    return alloc.proc(alloc.data, NkAllocator_QuerySpaceLeft, 0, 1, result, 0);
}

extern NkAllocator nk_default_allocator;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} NkArena;

NkAllocator nk_arena_getAllocator(NkArena *arena);

void *nk_arena_allocAligned(NkArena *arena, size_t size, uint8_t align);

NK_INLINE void *nk_arena_alloc(NkArena *arena, size_t size) {
    return nk_arena_allocAligned(arena, size, 1);
}

void nk_arena_pop(NkArena *arena, size_t size);

NK_INLINE void nk_arena_clear(NkArena *arena) {
    nk_arena_pop(arena, arena->size);
}

void nk_arena_free(NkArena *arena);

typedef struct {
    size_t size;
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
T *nk_alloc_t(NkAllocator alloc, size_t n = 1) {
    return (T *)nk_allocAligned(alloc, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_realloc_t(NkAllocator alloc, size_t n, T *old_mem, size_t old_n) {
    return (T *)nk_reallocAligned(alloc, n * sizeof(T), alignof(T), (void *)old_mem, old_n * sizeof(T));
}

template <class T>
void nk_free_t(NkAllocator alloc, T *ptr, size_t n = 1) {
    nk_freeAligned(alloc, ptr, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_arena_alloc_t(NkArena *arena, size_t n = 1) {
    return (T *)nk_arena_allocAligned(arena, n * sizeof(T), alignof(T));
}

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_ALLOCATOR
