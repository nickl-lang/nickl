#ifndef NTK_ALLOCATOR_H_
#define NTK_ALLOCATOR_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkAllocatorMode_Alloc,
    NkAllocatorMode_Free,
    NkAllocatorMode_Realloc,
    NkAllocatorMode_QuerySpaceLeft,
} NkAllocatorMode;

typedef void *(*NkAllocatorProc)(void *data, NkAllocatorMode mode, usize size, u8 align, void *old_mem, usize old_size);

typedef struct {
    void *data;
    NkAllocatorProc proc;
} NkAllocator;

typedef enum {
    NkAllocatorSpaceLeftQueryResultKind_Unknown,
    NkAllocatorSpaceLeftQueryResultKind_Limited,
} NkAllocatorSpaceLeftQueryResultKind;

typedef struct {
    NkAllocatorSpaceLeftQueryResultKind kind;
    usize bytes_left;
} NkAllocatorSpaceLeftQueryResult;

NK_INLINE void *nk_allocAligned(NkAllocator alloc, usize size, u8 align) {
    return alloc.proc(alloc.data, NkAllocatorMode_Alloc, size, align, NULL, 0);
}

NK_INLINE void nk_freeAligned(NkAllocator alloc, void *ptr, usize size, u8 align) {
    alloc.proc(alloc.data, NkAllocatorMode_Free, 0, align, ptr, size);
}

NK_INLINE void *nk_reallocAligned(NkAllocator alloc, usize size, u8 align, void *old_mem, usize old_size) {
    return alloc.proc(alloc.data, NkAllocatorMode_Realloc, size, align, old_mem, old_size);
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
    return alloc.proc(alloc.data, NkAllocatorMode_QuerySpaceLeft, 0, 1, result, 0);
}

extern NkAllocator nk_default_allocator;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

template <class T>
T *nk_allocT(NkAllocator alloc, usize n = 1) {
    return (T *)nk_allocAligned(alloc, n * sizeof(T), alignof(T));
}

template <class T>
T *nk_reallocT(NkAllocator alloc, usize n, T *old_mem, usize old_n) {
    return (T *)nk_reallocAligned(alloc, n * sizeof(T), alignof(T), (void *)old_mem, old_n * sizeof(T));
}

template <class T>
void nk_freeT(NkAllocator alloc, T *ptr, usize n = 1) {
    nk_freeAligned(alloc, ptr, n * sizeof(T), alignof(T));
}

#else

#define nk_allocT(alloc, T) (T *)nk_allocAligned((alloc), sizeof(T), alignof(T))
#define nk_freeT(alloc, ptr, T) nk_freeAligned((alloc), (ptr), sizeof(T), alignof(T))

#endif // __cplusplus

#endif // NTK_ALLOCATOR_H_
