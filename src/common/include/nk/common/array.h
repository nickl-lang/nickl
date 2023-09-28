#ifndef HEADER_GUARD_NK_COMMON_ARRAY
#define HEADER_GUARD_NK_COMMON_ARRAY

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/common/utils.h"
#include "nk/sys/common.h"

#define nkav_type(T) \
    struct {         \
        T *data;     \
        size_t size; \
    }

#define nkav_typedef(T, Name) typedef nkav_type(T) Name

#define nkav_init(obj) .data = (obj).data, .size = (obj).size,

#define nkar_type(T)       \
    struct {               \
        T *data;           \
        size_t size;       \
        size_t capacity;   \
        NkAllocator alloc; \
    }

#define nkar_typedef(T, Name) typedef nkar_type(T) Name

#ifndef NKAR_INIT_CAP
#define NKAR_INIT_CAP 16
#endif // NKAR_INIT_CAP

#define _nkar_maybe_grow(ar, cap)                                                             \
    do {                                                                                      \
        size_t _cap = (cap);                                                                  \
        if (_cap > (ar)->capacity) {                                                          \
            NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator; \
            void *const _new_data = nk_reallocAligned(                                        \
                _alloc,                                                                       \
                _cap * sizeof(*(ar)->data),                                                   \
                alignof(max_align_t),                                                         \
                (ar)->data,                                                                   \
                (ar)->capacity * sizeof(*(ar)->data));                                        \
            _nk_assign_void_ptr((ar)->data, _new_data);                                       \
            (ar)->capacity = _cap;                                                            \
        }                                                                                     \
    } while (0)

#define _nkar_reserve(ar, cap) _nkar_maybe_grow((ar), ceilToPowerOf2(maxu((cap), NKAR_INIT_CAP)))

#define _nkar_append(ar, item)              \
    do {                                    \
        nkar_reserve((ar), (ar)->size + 1); \
        (ar)->data[(ar)->size++] = (item);  \
    } while (0)

#define _nkar_append_many(ar, items, count)                                     \
    do {                                                                        \
        size_t _count = (count);                                                \
        nkar_reserve(ar, (ar)->size + _count);                                  \
        memcpy((ar)->data + (ar)->size, (items), _count * sizeof(*(ar)->data)); \
        (ar)->size += _count;                                                   \
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
void nkar_append_many(TAr *ar, T *items, size_t count) {
    _nkar_append_many(ar, items, count);
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
    TAr _ar;
    using pointer = decltype(TAr::data);
    pointer begin() {
        return _ar.data;
    }
    pointer end() {
        return _ar.data + _ar.size;
    }
};

#else // __cplusplus

#define _nk_assign_void_ptr(dst, src) (dst) = (src)

#define nkar_maybe_grow _nkar_maybe_grow
#define nkar_reserve _nkar_reserve
#define nkar_append _nkar_append
#define nkar_append_many _nkar_append_many
#define nkar_free _nkar_free
#define nkar_pop _nkar_pop
#define nkar_clear _nkar_clear

#endif // __cplusplus

#define nkar_last(ar) ((ar).data[(ar).size - 1])

#define nkar_push nkar_append

#endif // HEADER_GUARD_NK_COMMON_ARRAY
