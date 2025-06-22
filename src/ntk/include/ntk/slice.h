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

#define NKS_INIT(slice) .data = (slice).data, .size = (slice).size

#define nks_begin(slice) ((slice)->data)
#define nks_end(slice) ((slice)->data + (slice)->size)

#define nks_first(slice) ((slice).data[0])
#define nks_last(slice) ((slice).data[(slice).size - 1])

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
        return nks_begin(&_slice);
    }
    auto end() {
        return nks_end(&_slice);
    }
};

#else // __cplusplus

#define nk_slice_copy _nk_slice_copy

#endif // __cplusplus

#endif // NTK_SLICE_H_
