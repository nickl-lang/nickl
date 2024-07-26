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

#define NK_SLICE_INIT(slice) .data = (slice).data, .size = (slice).size

#define nk_slice_begin(slice) ((slice)->data)
#define nk_slice_end(slice) ((slice)->data + (slice)->size)

#define nk_slice_first(slice) ((slice).data[0])
#define nk_slice_last(slice) ((slice).data[(slice).size - 1])

#define _nk_slice_copy(alloc, dst, src)                                     \
    do {                                                                    \
        (dst)->size = (src).size;                                           \
        void *_mem = nk_alloc((alloc), (dst)->size * sizeof(*(dst)->data)); \
        memcpy(_mem, (src).data, (dst)->size * sizeof(*(dst)->data));       \
        _nk_assignVoidPtr((dst)->data, _mem);                               \
    } while (0)

#ifdef __cplusplus

template <class TDst, class TSrc>
void nk_slice_copy(NkAllocator alloc, TDst *dst, TSrc src) {
    _nk_slice_copy(alloc, dst, src);
}

#define nk_iterate(slice)         \
    _NkIterate<decltype(slice)> { \
        slice                     \
    }

template <class TSlice>
struct _NkIterate {
    TSlice const &_slice;
    auto begin() {
        return nk_slice_begin(&_slice);
    }
    auto end() {
        return nk_slice_end(&_slice);
    }
};

#else // __cplusplus

#define nk_slice_copy _nk_slice_copy

#endif // __cplusplus

#endif // NTK_SLICE_H_
