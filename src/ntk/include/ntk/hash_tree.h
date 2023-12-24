#ifndef HEADER_GUARD_NTK_HASH_TREE
#define HEADER_GUARD_NTK_HASH_TREE

#include <stddef.h>
#include <stdint.h>

#include "ntk/allocator.h"

#define nkht_type(T)       \
    struct {               \
        T *root;           \
        T *tmp;            \
        NkAllocator alloc; \
    }

#define nkht_typedef(T, Name) typedef nkht_type(T) Name

#ifdef __cplusplus
extern "C" {
#endif

void *_nkht_insert_impl(
    void **insert,
    NkAllocator alloc,
    void *elem,
    size_t elem_size,
    size_t key_size,
    size_t key_offset);

void _nkht_free_impl(void *root, NkAllocator alloc, size_t elem_size);

#define _nkht_insert_val(ht, item)  \
    _nk_assign_void_ptr(            \
        (ht).tmp,                   \
        _nkht_insert_impl(          \
            (void **)&(ht).root,    \
            (ht).alloc,             \
            &(item),                \
            sizeof(*(ht).root),     \
            sizeof((ht).root->key), \
            (uint8_t *)(ht).root - (uint8_t *)&(ht).root->key))

#define _nkht_insert_str(ht, item)

#define nkht_free(ht) _nkht_free_impl((void *)(ht).root, (ht).alloc, sizeof(*(ht).root));

#define nkht_insert_val _nkht_insert_val
#define nkht_insert_str _nkht_insert_str

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_HASH_TREE
