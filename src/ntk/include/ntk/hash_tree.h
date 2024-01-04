#ifndef HEADER_GUARD_NTK_HASH_TREE
#define HEADER_GUARD_NTK_HASH_TREE

#include <stddef.h>
#include <stdint.h>

#include "ntk/allocator.h"

#define nkht_type(T)       \
    struct {               \
        T *root;           \
        NkAllocator alloc; \
        union {            \
            T tmp_val;     \
            T *tmp_ptr;    \
        };                 \
    }

#define nkht_typedef(T, Name) typedef nkht_type(T) Name

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _nkht_hash_val,
    _nkht_hash_str,
} _nkht_hash_mode;

void *_nkht_insert_impl(
    void **root,
    NkAllocator alloc,
    void *elem,
    size_t elem_size,
    size_t key_size,
    size_t key_offset,
    _nkht_hash_mode mode);

void *_nkht_find_impl(
    void *root,
    size_t elem_size,
    void *key,
    size_t key_size,
    size_t key_offset,
    _nkht_hash_mode mode);

void _nkht_free_impl(void **root, NkAllocator alloc, size_t elem_size);

#ifdef __cplusplus
}
#endif

#define _nkht_insert_val(ht, item)                                \
    ((ht)->tmp_val = (item),                                      \
     _nk_assign_void_ptr(                                         \
         (ht)->tmp_ptr,                                           \
         _nkht_insert_impl(                                       \
             (void **)&(ht)->root,                                \
             (ht)->alloc,                                         \
             (void *)&(ht)->tmp_val,                              \
             sizeof(*(ht)->root),                                 \
             sizeof((ht)->root->key),                             \
             (uint8_t *)(ht)->root - (uint8_t *)&(ht)->root->key, \
             _nkht_hash_val)))

#define _nkht_insert_str(ht, item)                                       \
    ((ht)->tmp_val = (item),                                             \
     _nk_assign_void_ptr(                                                \
         (ht)->tmp_ptr,                                                  \
         _nkht_insert_impl(                                              \
             (void **)&(ht)->root,                                       \
             (ht)->alloc,                                                \
             (void *)&(ht)->tmp_val,                                     \
             sizeof(*(ht)->root),                                        \
             (ht)->tmp_val.key.size * sizeof((ht)->tmp_val.key.data[0]), \
             (uint8_t *)(ht)->root - (uint8_t *)&(ht)->root->key,        \
             _nkht_hash_str)))

#define _nkht_find_val(ht, KEY)                                   \
    ((ht)->tmp_val.key = (KEY),                                   \
     _nk_assign_void_ptr(                                         \
         (ht)->tmp_ptr,                                           \
         _nkht_find_impl(                                         \
             (void *)(ht)->root,                                  \
             sizeof(*(ht)->root),                                 \
             (void *)&(ht)->tmp_val.key,                          \
             sizeof((ht)->root->key),                             \
             (uint8_t *)(ht)->root - (uint8_t *)&(ht)->root->key, \
             _nkht_hash_val)))

#define _nkht_find_str(ht, KEY)                                          \
    ((ht)->tmp_val.key = (KEY),                                          \
     _nk_assign_void_ptr(                                                \
         (ht)->tmp_ptr,                                                  \
         _nkht_find_impl(                                                \
             (void *)(ht)->root,                                         \
             sizeof(*(ht)->root),                                        \
             (void *)(ht)->tmp_val.key.data,                             \
             (ht)->tmp_val.key.size * sizeof((ht)->tmp_val.key.data[0]), \
             (uint8_t *)(ht)->root - (uint8_t *)&(ht)->root->key,        \
             _nkht_hash_str)))

#define _nkht_free(ht) _nkht_free_impl((void **)&(ht)->root, (ht)->alloc, sizeof(*(ht)->root));

#ifdef __cplusplus

#include <utility>

template <class THt, class TItem>
auto nkht_insert_val(THt *ht, TItem &&item) -> decltype(THt::root) {
    return _nkht_insert_val(ht, std::forward<TItem>(item));
}

template <class THt, class TItem>
auto nkht_insert_str(THt *ht, TItem &&item) -> decltype(THt::root) {
    return _nkht_insert_str(ht, std::forward<TItem>(item));
}

template <class THt, class TKey>
auto nkht_find_val(THt *ht, TKey &&item) -> decltype(THt::root) {
    return _nkht_find_val(ht, std::forward<TKey>(item));
}

template <class THt, class TKey>
auto nkht_find_str(THt *ht, TKey &&item) -> decltype(THt::root) {
    return _nkht_find_str(ht, std::forward<TKey>(item));
}

template <class THt>
void nkht_free(THt *ht) {
    _nkht_free(ht);
}

#else // __cplusplus

#define nkht_insert_val _nkht_insert_val
#define nkht_insert_str _nkht_insert_str

#define nkht_find_val _nkht_find_val
#define nkht_find_str _nkht_find_str

#define nkht_free _nkht_free

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_HASH_TREE
