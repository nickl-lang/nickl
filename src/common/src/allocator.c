#include "nk/common/allocator.h"

extern inline void *nk_allocAligned(NkAllocator alloc, size_t size, uint8_t align);
extern inline void nk_freeAligned(NkAllocator alloc, void *ptr, size_t size, uint8_t align);
extern inline void *nk_reallocAligned(NkAllocator alloc, size_t size, uint8_t align, void *old_mem, size_t old_size);
extern inline void *nk_alloc(NkAllocator alloc, size_t size);
extern inline void nk_free(NkAllocator alloc, void *ptr, size_t size);
extern inline void *nk_realloc(NkAllocator alloc, size_t size, void *old_mem, size_t old_size);
extern inline void *nk_alloc_querySpaceLeft(NkAllocator alloc, NkAllocatorSpaceLeftQueryResult *result);
extern inline void *nk_arena_alloc(NkArena *arena, size_t size);
extern inline void nk_arena_clear(NkArena *arena);
extern inline NkArenaFrame nk_arena_grab(NkArena *arena);
extern inline void nk_arena_popFrame(NkArena *arena, NkArenaFrame frame);
