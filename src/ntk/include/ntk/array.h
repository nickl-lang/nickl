#ifndef HEADER_GUARD_NTK_ARRAY
#define HEADER_GUARD_NTK_ARRAY

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/utils.h"

#define nkav_type(T) \
    struct {         \
        T *data;     \
        size_t size; \
    }

#define nkav_typedef(T, Name) typedef nkav_type(T) Name

#define nkav_init(obj) .data = (obj).data, .size = (obj).size

#define nkav_begin(av) ((av)->data)
#define nkav_end(av) ((av)->data + (av)->size)

#define nkav_first(ar) ((ar).data[0])
#define nkav_last(ar) ((ar).data[(ar).size - 1])

#define nkar_push nkar_append

#define nkar_type(T)       \
    struct {               \
        T *data;           \
        size_t size;       \
        size_t capacity;   \
        NkAllocator alloc; \
    }

#define nkar_typedef(T, Name) typedef nkar_type(T) Name

#define _nkav_copy(alloc, dst, src)                                         \
    do {                                                                    \
        (dst)->size = (src).size;                                           \
        void *_mem = nk_alloc((alloc), (dst)->size * sizeof(*(dst)->data)); \
        memcpy(_mem, (src).data, (dst)->size * sizeof(*(dst)->data));       \
        _nk_assign_void_ptr((dst)->data, _mem);                             \
    } while (0)

#ifndef NKAR_INIT_CAP
#define NKAR_INIT_CAP 16
#endif // NKAR_INIT_CAP

#define _nkar_maybe_grow(ar, cap)                                                             \
    do {                                                                                      \
        size_t _cap = (cap);                                                                  \
        if (_cap > (ar)->capacity) {                                                          \
            NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator; \
            NkAllocatorSpaceLeftQueryResult _query_res;                                       \
            nk_alloc_querySpaceLeft(_alloc, &_query_res);                                     \
            if (_query_res.kind == NkAllocatorSpaceLeft_Limited) {                            \
                _cap = minu(_cap, (ar)->capacity + _query_res.bytes_left);                    \
            }                                                                                 \
            if (_cap > (ar)->capacity) {                                                      \
                void *const _new_data = nk_reallocAligned(                                    \
                    _alloc,                                                                   \
                    _cap * sizeof(*(ar)->data),                                               \
                    alignof(max_align_t),                                                     \
                    (ar)->data,                                                               \
                    (ar)->capacity * sizeof(*(ar)->data));                                    \
                _nk_assign_void_ptr((ar)->data, _new_data);                                   \
                (ar)->capacity = _cap;                                                        \
            }                                                                                 \
        }                                                                                     \
    } while (0)

#define _nkar_reserve(ar, cap) _nkar_maybe_grow((ar), ceilToPowerOf2(maxu((cap), NKAR_INIT_CAP)))

#define _nkar_append(ar, item)                                       \
    do {                                                             \
        nkar_reserve((ar), (ar)->size + 1);                          \
        assert((ar)->capacity - (ar)->size >= 1 && "no space left"); \
        (ar)->data[(ar)->size++] = (item);                           \
    } while (0)

#define _nkar_try_append(ar, item)              \
    do {                                        \
        nkar_reserve((ar), (ar)->size + 1);     \
        if ((ar)->capacity - (ar)->size >= 1) { \
            (ar)->data[(ar)->size++] = (item);  \
        }                                       \
    } while (0)

#define _nkar_append_many(ar, items, count)                               \
    do {                                                                  \
        size_t _count = (count);                                          \
        nkar_reserve(ar, (ar)->size + _count);                            \
        assert((ar)->capacity - (ar)->size >= _count && "no space left"); \
        memcpy(nkav_end(ar), (items), _count * sizeof(*(ar)->data));      \
        (ar)->size += _count;                                             \
    } while (0)

#define _nkar_try_append_many(ar, items, count)                      \
    do {                                                             \
        size_t _count = (count);                                     \
        nkar_reserve(ar, (ar)->size + _count);                       \
        _count = minu(_count, (ar)->capacity - (ar)->size);          \
        memcpy(nkav_end(ar), (items), _count * sizeof(*(ar)->data)); \
        (ar)->size += _count;                                        \
    } while (0)

#define _nkar_free(ar)                                                                                  \
    do {                                                                                                \
        NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator;               \
        nk_freeAligned(_alloc, (ar)->data, (ar)->capacity * sizeof(*(ar)->data), alignof(max_align_t)); \
        (ar)->data = NULL;                                                                              \
        (ar)->size = 0;                                                                                 \
        (ar)->capacity = 0;                                                                             \
    } while (0)

#define _nkar_pop(ar, count)                                                       \
    do {                                                                           \
        size_t _count = (count);                                                   \
        assert(_count <= (ar)->size && "trying to pop more items than available"); \
        (ar)->size -= _count;                                                      \
    } while (0)

#define _nkar_clear(ar) nkar_pop((ar), (ar)->size)

#ifdef __cplusplus

#include <utility>

template <class T>
void _nk_assign_void_ptr(T *&dst, void *src) {
    dst = (T *)src;
}

template <class TDst, class TSrc>
void nkav_copy(NkAllocator alloc, TDst *dst, TSrc src) {
    _nkav_copy(alloc, dst, src);
}

template <class TAr>
void nkar_maybe_grow(TAr *ar, size_t cap) {
    _nkar_maybe_grow(ar, cap);
}
template <class TAr>
void nkar_reserve(TAr *ar, size_t cap) {
    _nkar_reserve(ar, cap);
}
template <class TAr, class T>
void nkar_append(TAr *ar, T &&item) {
    _nkar_append(ar, std::forward<T>(item));
}
template <class TAr, class T>
void nkar_try_append(TAr *ar, T &&item) {
    _nkar_try_append(ar, std::forward<T>(item));
}
template <class TAr, class T>
void nkar_append_many(TAr *ar, T *items, size_t count) {
    _nkar_append_many(ar, items, count);
}
template <class TAr, class T>
void nkar_try_append_many(TAr *ar, T *items, size_t count) {
    _nkar_try_append_many(ar, items, count);
}
template <class TAr>
void nkar_free(TAr *ar) {
    _nkar_free(ar);
}
template <class TAr>
void nkar_pop(TAr *ar, size_t count) {
    _nkar_pop(ar, count);
}
template <class TAr>
void nkar_clear(TAr *ar) {
    _nkar_clear(ar);
}

#define nk_iterate(ar)                  \
    _nk_iterate_wrapper<decltype(ar)> { \
        ar                              \
    }

template <class TAr>
struct _nk_iterate_wrapper {
    TAr const &_ar;
    auto begin() {
        return nkav_begin(&_ar);
    }
    auto end() {
        return nkav_end(&_ar);
    }
};

#else // __cplusplus

#define _nk_assign_void_ptr(dst, src) (dst) = (src)

#define nkav_copy _nkav_copy

#define nkar_maybe_grow _nkar_maybe_grow
#define nkar_reserve _nkar_reserve
#define nkar_append _nkar_append
#define nkar_try_append _nkar_try_append
#define nkar_append_many _nkar_append_many
#define nkar_try_append_many _nkar_try_append_many
#define nkar_free _nkar_free
#define nkar_pop _nkar_pop
#define nkar_clear _nkar_clear

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_ARRAY
