#ifndef NTK_SLICE_H_
#define NTK_SLICE_H_

#include <string.h>

#include "ntk/allocator.h"
#include "ntk/common.h"

#define NkSlice(T)  \
    struct {        \
        T *data;    \
        usize size; \
    }

#define NkStridedSlice(T) \
    struct {              \
        T *data;          \
        usize size;       \
        usize stride;     \
    }

#define NKS_INIT(SLICE) .data = (SLICE).data, .size = (SLICE).size

#define NKS_BEGIN(SLICE) ((SLICE)->data)
#define NKS_END(SLICE) ((SLICE)->data + (SLICE)->size)

#define NKS_FIRST(SLICE) ((SLICE).data[0])
#define NKS_LAST(SLICE) ((SLICE).data[(SLICE).size - 1])

#define _NKS_COPY(ALLOC, DST, SRC)                                                      \
    do {                                                                                \
        if ((SRC).size) {                                                               \
            usize const _bytes = (SRC).size * sizeof(*(SRC).data);                      \
            void *_data = nk_allocAligned((ALLOC), _bytes, nk_alignofval(*(SRC).data)); \
            memcpy(_data, (SRC).data, _bytes);                                          \
            _nk_assignVoidPtr((DST)->data, _data);                                      \
        }                                                                               \
        (DST)->size = (SRC).size;                                                       \
    } while (0)

#define _NKS_COPY_STRIDED(ALLOC, DST, SRC)                                              \
    do {                                                                                \
        if ((SRC).size) {                                                               \
            usize const _bytes = (SRC).size * sizeof(*(SRC).data);                      \
            void *_data = nk_allocAligned((ALLOC), _bytes, nk_alignofval(*(SRC).data)); \
            u8 *_dst_it = (u8 *)_data;                                                  \
            u8 const *_src_it = (u8 const *)(SRC).data;                                 \
            while (_dst_it < (u8 *)_data + _bytes) {                                    \
                memcpy(_dst_it, _src_it, sizeof(*(SRC).data));                          \
                _dst_it += sizeof(*(SRC).data);                                         \
                _src_it += (SRC).stride;                                                \
            }                                                                           \
            _nk_assignVoidPtr((DST)->data, _data);                                      \
        }                                                                               \
        (DST)->size = (SRC).size;                                                       \
    } while (0)

#ifdef __cplusplus

template <class TDst, class TSrc>
void NKS_COPY(NkAllocator alloc, TDst *dst, TSrc src) {
    _NKS_COPY(alloc, dst, src);
}

template <class TDst, class TSrc>
void NKS_COPY_STRIDED(NkAllocator alloc, TDst *dst, TSrc src) {
    _NKS_COPY_STRIDED(alloc, dst, src);
}

#define nk_iterate(slice)         \
    _NkIterate<decltype(slice)> { \
        slice                     \
    }

template <class TSlice>
struct _NkIterate {
    TSlice const &_slice;
    auto begin() {
        return NKS_BEGIN(&_slice);
    }
    auto end() {
        return NKS_END(&_slice);
    }
};

#else // __cplusplus

#define NKS_COPY _NKS_COPY
#define NKS_COPY_STRIDED _NKS_COPY_STRIDED

#endif // __cplusplus

#define NK_ITERATE(TYPE, IT, SLICE) for (TYPE IT = (SLICE).data; IT < (SLICE).data + (SLICE).size; IT++)
#define NK_INDEX(IT, SLICE) (usize)((IT) - (SLICE).data)

#define NK_ITERATE_STRIDED(TYPE, IT, SLICE)                                                             \
    for (TYPE IT = (SLICE).data; IT < (TYPE)((u8 const *)(SLICE).data + (SLICE).size * (SLICE).stride); \
         IT = (TYPE)((u8 const *)IT + (SLICE).stride))

#endif // NTK_SLICE_H_
