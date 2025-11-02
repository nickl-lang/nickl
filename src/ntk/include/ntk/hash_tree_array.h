#ifndef NTK_HASH_TREE_ARRAY_H_
#define NTK_HASH_TREE_ARRAY_H_

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash.h"
#include "ntk/utils.h"

#define NK_HASH_TREE_ARRAY_TYPEDEF(TArray, TItem) typedef NkDynArray(TItem) TArray

#define NK_HASH_TREE_ARRAY_TYPEDEF_K(TArray, TKey) \
    typedef struct {                               \
        TKey key;                                  \
        size_t left;                               \
        size_t right;                              \
    } TArray##_Item;                               \
    NK_HASH_TREE_ARRAY_TYPEDEF(TArray, TArray##_Item)

#define NK_HASH_TREE_ARRAY_TYPEDEF_KV(TArray, TKey, TVal) \
    typedef struct {                                      \
        TKey key;                                         \
        TVal val;                                         \
        size_t left;                                      \
        size_t right;                                     \
    } TArray##_Item;                                      \
    NK_HASH_TREE_ARRAY_TYPEDEF(TArray, TArray##_Item)

#define _NK_HASH_TREE_ARRAY_PROTO(ATTR, TArray, TItem, TKey)    \
    ATTR TItem *TArray##_insertItem(TArray *items, TItem item); \
    ATTR TItem *TArray##_findItem(TArray *items, TKey key);     \
    ATTR void TArray##_free(TArray *items)

#define _NK_HASH_TREE_ARRAY_PROTO_K(ATTR, TArray, TKey)  \
    ATTR TKey *TArray##_insert(TArray *items, TKey key); \
    ATTR TKey *TArray##_find(TArray *items, TKey key);   \
    _NK_HASH_TREE_ARRAY_PROTO(ATTR, TArray, TArray##_Item, TKey)

#define _NK_HASH_TREE_ARRAY_PROTO_KV(ATTR, TArray, TKey, TVal)              \
    ATTR TArray##_Item *TArray##_insert(TArray *items, TKey key, TVal val); \
    ATTR TVal *TArray##_find(TArray *items, TKey key);                      \
    _NK_HASH_TREE_ARRAY_PROTO(ATTR, TArray, TArray##_Item, TKey)

#define NK_HASH_TREE_ARRAY_PROTO(TArray, TItem, TKey) _NK_HASH_TREE_ARRAY_PROTO(_NK_EMPTY, TArray, TItem, TKey)
#define NK_HASH_TREE_ARRAY_PROTO_K(TArray, TKey) _NK_HASH_TREE_ARRAY_PROTO_K(_NK_EMPTY, TArray, TKey)
#define NK_HASH_TREE_ARRAY_PROTO_KV(TArray, TKey, TVal) _NK_HASH_TREE_ARRAY_PROTO_KV(_NK_EMPTY, TArray, TKey, TVal)

#define NK_HASH_TREE_ARRAY_PROTO_EXPORT(TArray, TItem, TKey) _NK_HASH_TREE_ARRAY_PROTO(NK_EXPORT, TArray, TItem, TKey)
#define NK_HASH_TREE_ARRAY_PROTO_K_EXPORT(TArray, TKey) _NK_HASH_TREE_ARRAY_PROTO_K(NK_EXPORT, TArray, TKey)
#define NK_HASH_TREE_ARRAY_PROTO_KV_EXPORT(TArray, TKey, TVal) \
    _NK_HASH_TREE_ARRAY_PROTO_KV(NK_EXPORT, TArray, TKey, TVal)

#define NK_HASH_TREE_ARRAY_IMPL(TArray, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)           \
    typedef struct {                                                                                  \
        size_t *idx_ptr;                                                                              \
        bool existing;                                                                                \
    } _##TArray##_SearchResult;                                                                       \
                                                                                                      \
    static _##TArray##_SearchResult _##TArray##_findNode(TArray *items, size_t *null_idx, TKey key) { \
        size_t *idx_ptr = null_idx;                                                                   \
        if (!items->size) {                                                                           \
            return NK_LITERAL(_##TArray##_SearchResult){idx_ptr, false};                              \
        }                                                                                             \
        NkHash64 const hash = KeyHashFunc(key);                                                       \
        do {                                                                                          \
            TItem *node = &items->data[*idx_ptr];                                                     \
            TKey const existing_key = GetKeyFunc(node);                                               \
            NkHash64 const existing_hash = KeyHashFunc(existing_key);                                 \
            switch ((existing_hash < hash) - (hash < existing_hash)) {                                \
                case 0:                                                                               \
                    if (KeyEqualFunc(key, existing_key)) {                                            \
                        return NK_LITERAL(_##TArray##_SearchResult){idx_ptr, true};                   \
                    }                                                                                 \
                    NK_FALLTHROUGH;                                                                   \
                case -1:                                                                              \
                    idx_ptr = &node->left;                                                            \
                    break;                                                                            \
                case +1:                                                                              \
                    idx_ptr = &node->right;                                                           \
                    break;                                                                            \
            }                                                                                         \
        } while (*idx_ptr);                                                                           \
        return NK_LITERAL(_##TArray##_SearchResult){idx_ptr, false};                                  \
    }                                                                                                 \
                                                                                                      \
    TItem *TArray##_insertItem(TArray *items, TItem item) {                                           \
        TKey const key = GetKeyFunc(&item);                                                           \
        size_t null_idx = 0;                                                                          \
        _##TArray##_SearchResult const res = _##TArray##_findNode(items, &null_idx, key);             \
        if (res.existing) {                                                                           \
            return &items->data[*res.idx_ptr];                                                        \
        } else {                                                                                      \
            *res.idx_ptr = items->size;                                                               \
            nkda_append(items, item);                                                                 \
            NKS_LAST(*items).left = 0;                                                                \
            NKS_LAST(*items).right = 0;                                                               \
            return &NKS_LAST(*items);                                                                 \
        }                                                                                             \
    }                                                                                                 \
                                                                                                      \
    TItem *TArray##_findItem(TArray *items, TKey key) {                                               \
        size_t null_idx = 0;                                                                          \
        _##TArray##_SearchResult const res = _##TArray##_findNode(items, &null_idx, key);             \
        return res.existing ? &items->data[*res.idx_ptr] : NULL;                                      \
    }                                                                                                 \
                                                                                                      \
    void TArray##_free(TArray *items) {                                                               \
        nkda_free(items);                                                                             \
    }                                                                                                 \
                                                                                                      \
    _NK_NOP_TOPLEVEL

#define NK_HASH_TREE_ARRAY_IMPL_K(TArray, TKey, KeyHashFunc, KeyEqualFunc)             \
    static TKey _##TArray##_Item_GetKey(TArray##_Item const *item) {                   \
        return item->key;                                                              \
    }                                                                                  \
    TKey *TArray##_insert(TArray *items, TKey key) {                                   \
        return &TArray##_insertItem(items, NK_LITERAL(TArray##_Item){key, 0, 0})->key; \
    }                                                                                  \
    TKey *TArray##_find(TArray *items, TKey key) {                                     \
        TArray##_Item *found = TArray##_findItem(items, key);                          \
        return found ? &found->key : NULL;                                             \
    }                                                                                  \
    NK_HASH_TREE_ARRAY_IMPL(TArray, TArray##_Item, TKey, _##TArray##_Item_GetKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_ARRAY_IMPL_KV(TArray, TKey, TVal, KeyHashFunc, KeyEqualFunc)     \
    static TKey _##TArray##_Item_GetKey(TArray##_Item const *item) {                  \
        return item->key;                                                             \
    }                                                                                 \
    TArray##_Item *TArray##_insert(TArray *items, TKey key, TVal val) {               \
        return TArray##_insertItem(items, NK_LITERAL(TArray##_Item){key, val, 0, 0}); \
    }                                                                                 \
    TVal *TArray##_find(TArray *items, TKey key) {                                    \
        TArray##_Item *found = TArray##_findItem(items, key);                         \
        return found ? &found->val : NULL;                                            \
    }                                                                                 \
    NK_HASH_TREE_ARRAY_IMPL(TArray, TArray##_Item, TKey, _##TArray##_Item_GetKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_ARRAY_FWD(TArray, TItem, TKey) \
    NK_HASH_TREE_ARRAY_TYPEDEF(TArray, TItem);      \
    NK_HASH_TREE_ARRAY_PROTO(TArray, TItem, TKey)

#define NK_HASH_TREE_ARRAY_FWD_EXPORT(TArray, TItem, TKey) \
    NK_HASH_TREE_ARRAY_TYPEDEF(TArray, TItem);             \
    NK_HASH_TREE_ARRAY_PROTO_EXPORT(TArray, TItem, TKey)

#define NK_HASH_TREE_ARRAY_FWD_K(TArray, TKey)  \
    NK_HASH_TREE_ARRAY_TYPEDEF_K(TArray, TKey); \
    NK_HASH_TREE_ARRAY_PROTO_K(TArray, TKey)

#define NK_HASH_TREE_ARRAY_FWD_K_EXPORT(TArray, TKey) \
    NK_HASH_TREE_ARRAY_TYPEDEF_K(TArray, TKey);       \
    NK_HASH_TREE_ARRAY_PROTO_K_EXPORT(TArray, TKey)

#define NK_HASH_TREE_ARRAY_FWD_KV(TArray, TKey, TVal)  \
    NK_HASH_TREE_ARRAY_TYPEDEF_KV(TArray, TKey, TVal); \
    NK_HASH_TREE_ARRAY_PROTO_KV(TArray, TKey, TVal)

#define NK_HASH_TREE_ARRAY_FWD_KV_EXPORT(TArray, TKey, TVal) \
    NK_HASH_TREE_ARRAY_TYPEDEF_KV(TArray, TKey, TVal);       \
    NK_HASH_TREE_ARRAY_PROTO_KV_EXPORT(TArray, TKey, TVal)

#define NK_HASH_TREE_ARRAY_DEFINE(TArray, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_ARRAY_FWD(TArray, TItem, TKey);                                              \
    NK_HASH_TREE_ARRAY_IMPL(TArray, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_ARRAY_DEFINE_K(TArray, TKey, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_ARRAY_FWD_K(TArray, TKey);                                  \
    NK_HASH_TREE_ARRAY_IMPL_K(TArray, TKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_ARRAY_DEFINE_KV(TArray, TKey, TVal, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_ARRAY_FWD_KV(TArray, TKey, TVal);                                  \
    NK_HASH_TREE_ARRAY_IMPL_KV(TArray, TKey, TVal, KeyHashFunc, KeyEqualFunc)

#endif // NTK_HASH_TREE_ARRAY_H_
