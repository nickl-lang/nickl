#ifndef HEADER_GUARD_NK_COMMON_ARRAY_H
#define HEADER_GUARD_NK_COMMON_ARRAY_H

#include <stdalign.h>
#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/common/utils.h"

#define nkar_typedef(T, Name) \
    typedef struct {          \
        T *data;              \
        size_t size;          \
        size_t capacity;      \
        NkAllocator alloc;    \
    } Name

#define nkar_reserve(ar, n)                                                                   \
    do {                                                                                      \
        if ((ar)->size + n > (ar)->capacity) {                                                \
            size_t const _new_capacity = ceilToPowerOf2((ar)->size + n);                      \
            NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator; \
            void *const _new_data = nk_reallocAligned(                                        \
                _alloc,                                                                       \
                _new_capacity * sizeof(*(ar)->data),                                          \
                alignof(max_align_t),                                                         \
                (ar)->data,                                                                   \
                (ar)->capacity * sizeof(*(ar)->data));                                        \
            nk_assign_void_ptr((ar)->data, _new_data);                                        \
            (ar)->capacity = _new_capacity;                                                   \
        }                                                                                     \
    } while (0)

#define nkar_append(ar, item)              \
    do {                                   \
        nkar_reserve(ar, 1);               \
        (ar)->data[(ar)->size++] = (item); \
    } while (0)

#define nkar_append_many(ar, items, count)                                   \
    do {                                                                     \
        nkar_reserve(ar, count);                                             \
        memcpy((ar)->data + (ar)->size, items, count * sizeof(*(ar)->data)); \
        (ar)->size += count;                                                 \
    } while (0)

#define nkar_free(ar)                                                                                   \
    do {                                                                                                \
        NkAllocator const _alloc = (ar)->alloc.proc ? (ar)->alloc : nk_default_allocator;               \
        nk_freeAligned(_alloc, (ar)->data, (ar)->capacity * sizeof(*(ar)->data), alignof(max_align_t)); \
        (ar)->data = NULL;                                                                              \
        (ar)->size = 0;                                                                                 \
        (ar)->capacity = 0;                                                                             \
    } while (0)

#ifdef __cplusplus
template <class T>
void nk_assign_void_ptr(T *&dst, void *src) {
    dst = (T *)src;
}
#else // __cplusplus
#define nk_assign_void_ptr(dst, src) (dst) = (src)
#endif // __cplusplus

#endif // HEADER_GUARD_NK_COMMON_ARRAY_H
