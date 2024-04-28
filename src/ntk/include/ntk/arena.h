#ifndef NTK_ARENA_H_
#define NTK_ARENA_H_

#include "ntk/allocator.h"
#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    return NK_LITERAL(NkArenaFrame){arena->size};
}

NK_INLINE void nk_arena_popFrame(NkArena *arena, NkArenaFrame frame) {
    nk_arena_pop(arena, arena->size - frame.size);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

template <class T>
T *nk_arena_allocT(NkArena *arena, usize n = 1) {
    return (T *)nk_arena_allocAligned(arena, n * sizeof(T), alignof(T));
}

#else // __cplusplus

#define nk_arena_allocT(arena, T) (T *)nk_arena_allocAligned((arena), sizeof(T), alignof(T))

#endif // __cplusplus

#define NK_ARENA_SCOPE(arena)               \
    NkArenaFrame NK_CAT(_frame_, __LINE__); \
    NK_DEFER_LOOP(NK_CAT(_frame_, __LINE__) = nk_arena_grab(arena), nk_arena_popFrame(arena, NK_CAT(_frame_, __LINE__)))

#endif // NTK_ARENA_H_
