#ifndef NTK_HASH_TREE_H_
#define NTK_HASH_TREE_H_

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/utils.h"

#define NK_HASH_TREE_TYPEDEF(TTree, TItem)          \
    typedef struct _##TTree##_Node _##TTree##_Node; \
    struct _##TTree##_Node {                        \
        TItem item;                                 \
        u64 hash;                                   \
        _##TTree##_Node *child[2];                  \
    };                                              \
    typedef struct {                                \
        _##TTree##_Node *root;                      \
        NkAllocator alloc;                          \
    } TTree

#define NK_HASH_TREE_TYPEDEF_K(TTree, TItem) NK_HASH_TREE_TYPEDEF(TTree, TItem)

#define NK_HASH_TREE_TYPEDEF_KV(TTree, TKey, TVal) \
    typedef struct {                               \
        TKey key;                                  \
        TVal val;                                  \
    } TTree##_Item;                                \
    NK_HASH_TREE_TYPEDEF(TTree, TTree##_Item)

#define _NK_HASH_TREE_PROTO(ATTR, TTree, TItem, TKey)      \
    ATTR TItem *TTree##_insertItem(TTree *ht, TItem item); \
    ATTR TItem *TTree##_findItem(TTree *ht, TKey key);     \
    ATTR void TTree##_free(TTree *ht)

#define _NK_HASH_TREE_PROTO_K(ATTR, TTree, TKey)    \
    ATTR TKey *TTree##_insert(TTree *ht, TKey key); \
    ATTR TKey *TTree##_find(TTree *ht, TKey key);   \
    _NK_HASH_TREE_PROTO(ATTR, TTree, TKey, TKey)

#define _NK_HASH_TREE_PROTO_KV(ATTR, TTree, TKey, TVal)               \
    ATTR TTree##_Item *TTree##_insert(TTree *ht, TKey key, TVal val); \
    ATTR TVal *TTree##_find(TTree *ht, TKey key);                     \
    _NK_HASH_TREE_PROTO(ATTR, TTree, TTree##_Item, TKey)

#define NK_HASH_TREE_PROTO(TTree, TItem, TKey) _NK_HASH_TREE_PROTO(_NK_EMPTY, TTree, TItem, TKey)
#define NK_HASH_TREE_PROTO_K(TTree, TKey) _NK_HASH_TREE_PROTO_K(_NK_EMPTY, TTree, TKey)
#define NK_HASH_TREE_PROTO_KV(TTree, TKey, TVal) _NK_HASH_TREE_PROTO_KV(_NK_EMPTY, TTree, TKey, TVal)

#define NK_HASH_TREE_PROTO_EXPORT(TTree, TItem, TKey) _NK_HASH_TREE_PROTO(NK_EXPORT, TTree, TItem, TKey)
#define NK_HASH_TREE_PROTO_K_EXPORT(TTree, TKey) _NK_HASH_TREE_PROTO_K(NK_EXPORT, TTree, TKey)
#define NK_HASH_TREE_PROTO_KV_EXPORT(TTree, TKey, TVal) _NK_HASH_TREE_PROTO_KV(NK_EXPORT, TTree, TKey, TVal)

#define NK_HASH_TREE_IMPL(TTree, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)                        \
    typedef struct {                                                                                        \
        _##TTree##_Node **node;                                                                             \
        bool existing;                                                                                      \
    } _##TTree##_SearchResult;                                                                              \
                                                                                                            \
    static _##TTree##_SearchResult _##TTree##_findNode(_##TTree##_Node **node, TKey const *key, u64 hash) { \
        while (*node) {                                                                                     \
            switch ((hash > (*node)->hash) - (hash < (*node)->hash)) {                                      \
                case 0: {                                                                                   \
                    TKey const *existing_key = GetKeyFunc(&(*node)->item);                                  \
                    if (KeyEqualFunc(*key, *existing_key)) {                                                \
                        return NK_LITERAL(_##TTree##_SearchResult){node, true};                             \
                    }                                                                                       \
                    NK_FALLTHROUGH;                                                                         \
                }                                                                                           \
                case -1:                                                                                    \
                    node = (*node)->child + 0;                                                              \
                    break;                                                                                  \
                case +1:                                                                                    \
                    node = (*node)->child + 1;                                                              \
                    break;                                                                                  \
            }                                                                                               \
        }                                                                                                   \
        return NK_LITERAL(_##TTree##_SearchResult){node, false};                                            \
    }                                                                                                       \
                                                                                                            \
    TItem *TTree##_insertItem(TTree *ht, TItem item) {                                                      \
        TKey const *key = GetKeyFunc(&item);                                                                \
        u64 hash = KeyHashFunc(*key);                                                                       \
        _##TTree##_SearchResult res = _##TTree##_findNode(&ht->root, key, hash);                            \
        if (!res.existing) {                                                                                \
            NkAllocator _alloc = ht->alloc.proc ? ht->alloc : nk_default_allocator;                         \
            *res.node = (_##TTree##_Node *)nk_allocAligned(                                                 \
                _alloc, sizeof(_##TTree##_Node), nk_maxu(alignof(TItem), alignof(_##TTree##_Node)));        \
            **res.node = NK_LITERAL(_##TTree##_Node){                                                       \
                .item = item,                                                                               \
                .hash = hash,                                                                               \
                .child = {0},                                                                               \
            };                                                                                              \
        }                                                                                                   \
        return (TItem *)*res.node;                                                                          \
    }                                                                                                       \
                                                                                                            \
    TItem *TTree##_findItem(TTree *ht, TKey key) {                                                          \
        u64 hash = KeyHashFunc(key);                                                                        \
        _##TTree##_SearchResult res = _##TTree##_findNode(&ht->root, &key, hash);                           \
        return (TItem *)*res.node;                                                                          \
    }                                                                                                       \
                                                                                                            \
    static void _##TTree##_freeNode(NkAllocator alloc, _##TTree##_Node *node) {                             \
        if (!node) {                                                                                        \
            return;                                                                                         \
        }                                                                                                   \
        _##TTree##_freeNode(alloc, node->child[0]);                                                         \
        _##TTree##_freeNode(alloc, node->child[1]);                                                         \
        nk_free(alloc, node, sizeof(_##TTree##_Node));                                                      \
    }                                                                                                       \
                                                                                                            \
    void TTree##_free(TTree *ht) {                                                                          \
        NkAllocator _alloc = ht->alloc.proc ? ht->alloc : nk_default_allocator;                             \
        _##TTree##_freeNode(_alloc, ht->root);                                                              \
        ht->root = NULL;                                                                                    \
    }                                                                                                       \
                                                                                                            \
    _NK_NOP_TOPLEVEL

#define NK_HASH_TREE_IMPL_K(TTree, TKey, KeyHashFunc, KeyEqualFunc) \
    static TKey const *_##TTree##_Item_GetKey(TKey const *key) {    \
        return key;                                                 \
    }                                                               \
    TKey *TTree##_insert(TTree *ht, TKey key) {                     \
        return TTree##_insertItem(ht, key);                         \
    }                                                               \
    TKey *TTree##_find(TTree *ht, TKey key) {                       \
        return TTree##_findItem(ht, key);                           \
    }                                                               \
    NK_HASH_TREE_IMPL(TTree, TKey, TKey, _##TTree##_Item_GetKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_IMPL_KV(TTree, TKey, TVal, KeyHashFunc, KeyEqualFunc) \
    static TKey const *_##TTree##_Item_GetKey(TTree##_Item const *item) {  \
        return &item->key;                                                 \
    }                                                                      \
    TTree##_Item *TTree##_insert(TTree *ht, TKey key, TVal val) {          \
        return TTree##_insertItem(ht, NK_LITERAL(TTree##_Item){key, val}); \
    }                                                                      \
    TVal *TTree##_find(TTree *ht, TKey key) {                              \
        TTree##_Item *found = TTree##_findItem(ht, key);                   \
        return found ? &found->val : NULL;                                 \
    }                                                                      \
    NK_HASH_TREE_IMPL(TTree, TTree##_Item, TKey, _##TTree##_Item_GetKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_FWD(TTree, TItem, TKey) \
    NK_HASH_TREE_TYPEDEF(TTree, TItem);      \
    NK_HASH_TREE_PROTO(TTree, TItem, TKey);

#define NK_HASH_TREE_FWD_EXPORT(TTree, TItem, TKey) \
    NK_HASH_TREE_TYPEDEF(TTree, TItem);             \
    NK_HASH_TREE_PROTO_EXPORT(TTree, TItem, TKey);

#define NK_HASH_TREE_FWD_K(TTree, TKey)  \
    NK_HASH_TREE_TYPEDEF_K(TTree, TKey); \
    NK_HASH_TREE_PROTO_K(TTree, TKey);

#define NK_HASH_TREE_FWD_K_EXPORT(TTree, TKey) \
    NK_HASH_TREE_TYPEDEF_K(TTree, TKey);       \
    NK_HASH_TREE_PROTO_K_EXPORT(TTree, TKey);

#define NK_HASH_TREE_FWD_KV(TTree, TKey, TVal)  \
    NK_HASH_TREE_TYPEDEF_KV(TTree, TKey, TVal); \
    NK_HASH_TREE_PROTO_KV(TTree, TKey, TVal);

#define NK_HASH_TREE_FWD_KV_EXPORT(TTree, TKey, TVal) \
    NK_HASH_TREE_TYPEDEF_KV(TTree, TKey, TVal);       \
    NK_HASH_TREE_PROTO_KV_EXPORT(TTree, TKey, TVal);

#define NK_HASH_TREE_DEFINE(TTree, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_FWD(TTree, TItem, TKey);                                              \
    NK_HASH_TREE_IMPL(TTree, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_DEFINE_K(TTree, TKey, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_FWD_K(TTree, TKey);                                  \
    NK_HASH_TREE_IMPL_K(TTree, TKey, KeyHashFunc, KeyEqualFunc)

#define NK_HASH_TREE_DEFINE_KV(TTree, TKey, TVal, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_FWD_KV(TTree, TKey, TVal);                                  \
    NK_HASH_TREE_IMPL_KV(TTree, TKey, TVal, KeyHashFunc, KeyEqualFunc)

#ifdef __cplusplus
extern "C" {
#endif

NK_HASH_TREE_FWD_K_EXPORT(NkIntptrHashSet, intptr_t);
NK_HASH_TREE_FWD_KV_EXPORT(NkIntptrHashMap, intptr_t, intptr_t);

#ifdef __cplusplus
}
#endif

#endif // NTK_HASH_TREE_H_
