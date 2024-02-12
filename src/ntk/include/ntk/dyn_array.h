#ifndef NTK_DYN_ARRAY_H_
#define NTK_DYN_ARRAY_H_

#include <string.h>

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/slice.h"
#include "ntk/utils.h"

#define NkDynArray(T)      \
    struct {               \
        T *data;           \
        usize size;        \
        usize capacity;    \
        NkAllocator alloc; \
    }

#ifndef NKDA_INITIAL_CAPACITY
#define NKDA_INITIAL_CAPACITY 16
#endif // NKDA_INITIAL_CAPACITY

#define _nkda_maybeGrow(ar, cap)                                                              \
    do {                                                                                      \
        usize _cap = (cap);                                                                   \
        if (_cap > (ar)->capacity) {                                                          \
            NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator; \
            NkAllocatorSpaceLeftQueryResult _query_res;                                       \
            nk_alloc_querySpaceLeft(_alloc, &_query_res);                                     \
            if (_query_res.kind == NkAllocatorSpaceLeftQueryResultKind_Limited) {             \
                _cap = nk_minu(_cap, (ar)->capacity + _query_res.bytes_left);                 \
            }                                                                                 \
            if (_cap > (ar)->capacity) {                                                      \
                void *const _new_data = nk_reallocAligned(                                    \
                    _alloc,                                                                   \
                    _cap * sizeof(*(ar)->data),                                               \
                    alignof(max_align_t),                                                     \
                    (ar)->data,                                                               \
                    (ar)->capacity * sizeof(*(ar)->data));                                    \
                _nk_assignVoidPtr((ar)->data, _new_data);                                     \
                (ar)->capacity = _cap;                                                        \
            }                                                                                 \
        }                                                                                     \
    } while (0)

#define _nkda_reserve(ar, cap) _nkda_maybeGrow((ar), nk_ceilToPowerOf2(nk_maxu((cap), NKDA_INITIAL_CAPACITY)))

#define _nkda_append(ar, item)                                          \
    do {                                                                \
        nkda_reserve((ar), (ar)->size + 1);                             \
        nk_assert((ar)->capacity - (ar)->size >= 1 && "no space left"); \
        (ar)->data[(ar)->size++] = (item);                              \
    } while (0)

#define _nkda_tryAppend(ar, item)               \
    do {                                        \
        nkda_reserve((ar), (ar)->size + 1);     \
        if ((ar)->capacity - (ar)->size >= 1) { \
            (ar)->data[(ar)->size++] = (item);  \
        }                                       \
    } while (0)

#define _nkda_appendMany(ar, items, count)                                   \
    do {                                                                     \
        usize _count = (count);                                              \
        nkda_reserve(ar, (ar)->size + _count);                               \
        nk_assert((ar)->capacity - (ar)->size >= _count && "no space left"); \
        memcpy(nk_slice_end(ar), (items), _count * sizeof(*(ar)->data));     \
        (ar)->size += _count;                                                \
    } while (0)

#define _nkda_tryAppendMany(ar, items, count)                            \
    do {                                                                 \
        usize _count = (count);                                          \
        nkda_reserve(ar, (ar)->size + _count);                           \
        _count = nk_minu(_count, (ar)->capacity - (ar)->size);           \
        memcpy(nk_slice_end(ar), (items), _count * sizeof(*(ar)->data)); \
        (ar)->size += _count;                                            \
    } while (0)

#define _nkda_free(ar)                                                                                  \
    do {                                                                                                \
        NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator;               \
        nk_freeAligned(_alloc, (ar)->data, (ar)->capacity * sizeof(*(ar)->data), alignof(max_align_t)); \
        (ar)->data = NULL;                                                                              \
        (ar)->size = 0;                                                                                 \
        (ar)->capacity = 0;                                                                             \
    } while (0)

#define _nkda_pop(ar, count)                                                          \
    do {                                                                              \
        usize _count = (count);                                                       \
        nk_assert(_count <= (ar)->size && "trying to pop more items than available"); \
        (ar)->size -= _count;                                                         \
    } while (0)

#define _nkda_clear(ar) nkda_pop((ar), (ar)->size)

#ifdef __cplusplus

#include <utility>

template <class TAr>
void nkda_maybeGrow(TAr *ar, usize cap) {
    _nkda_maybeGrow(ar, cap);
}
template <class TAr>
void nkda_reserve(TAr *ar, usize cap) {
    _nkda_reserve(ar, cap);
}
template <class TAr, class T>
void nkda_append(TAr *ar, T &&item) {
    _nkda_append(ar, std::forward<T>(item));
}
template <class TAr, class T>
void nkda_tryAppend(TAr *ar, T &&item) {
    _nkda_tryAppend(ar, std::forward<T>(item));
}
template <class TAr, class T>
void nkda_appendMany(TAr *ar, T *items, usize count) {
    _nkda_appendMany(ar, items, count);
}
template <class TAr, class T>
void nkda_tryAppendMany(TAr *ar, T *items, usize count) {
    _nkda_tryAppendMany(ar, items, count);
}
template <class TAr>
void nkda_free(TAr *ar) {
    _nkda_free(ar);
}
template <class TAr>
void nkda_pop(TAr *ar, usize count) {
    _nkda_pop(ar, count);
}
template <class TAr>
void nkda_clear(TAr *ar) {
    _nkda_clear(ar);
}

#else // __cplusplus

#define nkda_maybeGrow _nkda_maybeGrow
#define nkda_reserve _nkda_reserve
#define nkda_append _nkda_append
#define nkda_tryAppend _nkda_tryAppend
#define nkda_appendMany _nkda_appendMany
#define nkda_tryAppendMany _nkda_tryAppendMany
#define nkda_free _nkda_free
#define nkda_pop _nkda_pop
#define nkda_clear _nkda_clear

#endif // __cplusplus

#define nkda_push nkda_append

#endif // NTK_DYN_ARRAY_H_
