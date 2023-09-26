#ifndef HEADER_GUARD_NK_COMMON_ARRAY_H
#define HEADER_GUARD_NK_COMMON_ARRAY_H

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/common/utils.h"
#include "nk/sys/common.h"

#define nkslice_type(T) \
    struct {            \
        T *data;        \
        size_t size;    \
    }

#define nkslice_typedef(T, Name) typedef nkslice_type(T) Name

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

#define nkar_create(ar_t, allocator)                                 \
    LITERAL(ar_t) {                                                  \
        .data = NULL, .size = 0, .capacity = 0, .alloc = (allocator) \
    }

#define nkar_maybe_grow(ar, cap)                                                              \
    do {                                                                                      \
        if ((cap) > (ar)->capacity) {                                                         \
            NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator; \
            void *const _new_data = nk_reallocAligned(                                        \
                _alloc,                                                                       \
                (cap) * sizeof(*(ar)->data),                                                  \
                alignof(max_align_t),                                                         \
                (ar)->data,                                                                   \
                (ar)->capacity * sizeof(*(ar)->data));                                        \
            nk_assign_void_ptr((ar)->data, _new_data);                                        \
            (ar)->capacity = (cap);                                                           \
        }                                                                                     \
    } while (0)

#define nkar_reserve(ar, cap) nkar_maybe_grow((ar), ceilToPowerOf2(maxu((cap), NKAR_INIT_CAP)))

#define nkar_append(ar, item)               \
    do {                                    \
        nkar_reserve((ar), (ar)->size + 1); \
        (ar)->data[(ar)->size++] = (item);  \
    } while (0)

#define nkar_append_many(ar, items, count)                                       \
    do {                                                                         \
        nkar_reserve(ar, (ar)->size + (count));                                  \
        memcpy((ar)->data + (ar)->size, (items), (count) * sizeof(*(ar)->data)); \
        (ar)->size += (count);                                                   \
    } while (0)

#define nkar_free(ar)                                                                                   \
    do {                                                                                                \
        NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator;               \
        nk_freeAligned(_alloc, (ar)->data, (ar)->capacity * sizeof(*(ar)->data), alignof(max_align_t)); \
        (ar)->data = NULL;                                                                              \
        (ar)->size = 0;                                                                                 \
        (ar)->capacity = 0;                                                                             \
    } while (0)

#define nkar_push nkar_append

#define nkar_pop(ar, count)                                                         \
    do {                                                                            \
        assert((count) <= (ar)->size && "trying to pop more items than available"); \
        (ar)->size -= (count);                                                      \
    } while (0)

#define nkar_clear(ar) nkar_pop((ar), (ar)->size)

#ifdef __cplusplus
template <class T>
void nk_assign_void_ptr(T *&dst, void *src) {
    dst = (T *)src;
}
#else // __cplusplus
#define nk_assign_void_ptr(dst, src) (dst) = (src)
#endif // __cplusplus

#define nk_ar_last(ar) ((ar).data[(ar).size - 1])

#endif // HEADER_GUARD_NK_COMMON_ARRAY_H
