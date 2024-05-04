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

#define NK_HASH_TREE_PROTO(TTree, TItem, TKey)          \
    TItem *TTree##_insert(TTree *ht, TItem const item); \
    TItem *TTree##_find(TTree *ht, TKey const key);     \
    void TTree##_free(TTree *ht)

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
                } /*fallthrough*/                                                                           \
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
    TItem *TTree##_insert(TTree *ht, TItem const item) {                                                    \
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
        return &(*res.node)->item;                                                                          \
    }                                                                                                       \
                                                                                                            \
    TItem *TTree##_find(TTree *ht, TKey const key) {                                                        \
        u64 hash = KeyHashFunc(key);                                                                        \
        _##TTree##_SearchResult res = _##TTree##_findNode(&ht->root, &key, hash);                           \
        return &(*res.node)->item;                                                                          \
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

#define NK_HASH_TREE_DEFINE(TTree, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_TYPEDEF(TTree, TItem);                                                \
    NK_HASH_TREE_PROTO(TTree, TItem, TKey);                                            \
    NK_HASH_TREE_IMPL(TTree, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)

#endif // NTK_HASH_TREE_H_
