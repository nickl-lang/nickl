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
        T *strided_data;  \
        usize size;       \
        usize stride;     \
    }

#define NKS_INIT(SLICE) .data = (SLICE).data, .size = (SLICE).size
#define NKS_INIT_STATIC(AR) .data = (AR), .size = NK_ARRAY_COUNT(AR)

#define NKS_INIT_STRIDED(SLICE) .strided_data = (SLICE).data, .size = (SLICE).size, .stride = sizeof(*(SLICE).data)

#define NKS_INIT_STRIDED_STATIC(AR) .strided_data = (AR), .size = NK_ARRAY_COUNT(AR), .stride = sizeof(*(AR))

#define NKS_INIT_STRIDED_FROM_FIELD(SLICE, FIELD) \
    .strided_data = &(SLICE).data->FIELD, .size = (SLICE).size, .stride = sizeof(*(SLICE).data)
#define NKS_INIT_STRIDED_FROM_FIELD_STATIC(AR, FIELD) \
    .strided_data = &(AR)->FIELD, .size = NK_ARRAY_COUNT(AR), .stride = sizeof(*(AR))

#define NKS_BEGIN(SLICE) ((SLICE)->data)
#define NKS_END(SLICE) ((SLICE)->data + (SLICE)->size)

#define NKS_FIRST(SLICE) ((SLICE).data[0])
#define NKS_LAST(SLICE) ((SLICE).data[(SLICE).size - 1])

#define NKS_BYTE_SIZE(SLICE) ((SLICE).size * sizeof(*(SLICE).data))
#define NKS_BYTE_SIZE_STRIDED(SLICE) ((SLICE).size * sizeof(*(SLICE).strided_data))

#define NKS_ZERO(SLICE) memset((SLICE).data, 0, NKS_BYTE_SIZE(SLICE))

#define _NKS_COPY(ALLOC, DST, SRC)                                                      \
    do {                                                                                \
        if ((SRC).size) {                                                               \
            usize const _bytes = NKS_BYTE_SIZE(SRC);                                    \
            void *_data = nk_allocAligned((ALLOC), _bytes, nk_alignofval(*(SRC).data)); \
            memcpy(_data, (SRC).data, _bytes);                                          \
            _nk_assignVoidPtr((DST)->data, _data);                                      \
        }                                                                               \
        (DST)->size = (SRC).size;                                                       \
    } while (0)

#define _NKS_COPY_STRIDED(ALLOC, DST, SRC)                                                      \
    do {                                                                                        \
        if ((SRC).size) {                                                                       \
            usize const _bytes = NKS_BYTE_SIZE_STRIDED(SRC);                                    \
            void *_data = nk_allocAligned((ALLOC), _bytes, nk_alignofval(*(SRC).strided_data)); \
            u8 *_dst_it = (u8 *)_data;                                                          \
            u8 const *_src_it = (u8 const *)(SRC).strided_data;                                 \
            while (_dst_it < (u8 *)_data + _bytes) {                                            \
                memcpy(_dst_it, _src_it, sizeof(*(SRC).strided_data));                          \
                _dst_it += sizeof(*(SRC).strided_data);                                         \
                _src_it += (SRC).stride;                                                        \
            }                                                                                   \
            _nk_assignVoidPtr((DST)->data, _data);                                              \
        }                                                                                       \
        (DST)->size = (SRC).size;                                                               \
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

#define NK_ITERATE_STRIDED(TYPE, IT, SLICE)                                             \
    for (TYPE IT = (SLICE).strided_data;                                                \
         IT < (TYPE)((u8 const *)(SLICE).strided_data + (SLICE).size * (SLICE).stride); \
         IT = (TYPE)((u8 const *)IT + (SLICE).stride))
#define NK_INDEX_STRIDED(IT, SLICE) (usize)(((u8 const *)(IT) - (u8 const *)(SLICE).strided_data) / (SLICE).stride)

#endif // NTK_SLICE_H_
