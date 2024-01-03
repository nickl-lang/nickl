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

void *_nkht_insert_impl(void **root, NkAllocator alloc, void *elem, size_t elemsize, size_t keysize, size_t keyoffset);

void _nkht_free_impl(void *root, NkAllocator alloc, size_t elemsize);

#ifdef __cplusplus
}
#endif

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

#define _nkht_free(ht) _nkht_free_impl((void *)(ht).root, (ht).alloc, sizeof(*(ht).root));

#ifdef __cplusplus

#include <utility>

template <class THt, class T>
T *nkht_insert_val(THt &ht, T &item) {
    return _nkht_insert_val(ht, item);
}

template <class THt, class T>
T *nkht_insert_str(THt &ht, T &item) {
    // return _nkht_insert_str(ht, item);
}

template <class THt>
void nkht_free(THt ht) {
    _nkht_free(ht);
}

#else // __cplusplus

#define nkht_insert_val _nkht_insert_val
#define nkht_insert_str _nkht_insert_str

#define nkht_free _nkht_free

#endif // __cplusplus

#endif // HEADER_GUARD_NTK_HASH_TREE
