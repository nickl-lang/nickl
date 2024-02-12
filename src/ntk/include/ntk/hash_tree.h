#ifndef NTK_HASH_TREE_H_
#define NTK_HASH_TREE_H_

#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/utils.h"

#define NK_HASH_TREE_TYPEDEF(Name, TItem)         \
    typedef struct _##Name##_Node _##Name##_Node; \
    struct _##Name##_Node {                       \
        TItem item;                               \
        u64 hash;                                 \
        _##Name##_Node *child[2];                 \
    };                                            \
    typedef struct {                              \
        _##Name##_Node *root;                     \
        NkAllocator alloc;                        \
    } Name

#define NK_HASH_TREE_PROTO(Name, TItem, TKey)         \
    TItem *Name##_insert(Name *ht, TItem const item); \
    TItem *Name##_find(Name *ht, TKey const key);     \
    void Name##_free(Name *ht)

#define NK_HASH_TREE_IMPL(Name, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)                      \
    typedef struct {                                                                                     \
        _##Name##_Node **node;                                                                           \
        bool existing;                                                                                   \
    } _##Name##_SearchResult;                                                                            \
                                                                                                         \
    static _##Name##_SearchResult _##Name##_findNode(_##Name##_Node **node, TKey const *key, u64 hash) { \
        while (*node) {                                                                                  \
            switch ((hash > (*node)->hash) - (hash < (*node)->hash)) {                                   \
            case 0: {                                                                                    \
                TKey const *existing_key = GetKeyFunc(&(*node)->item);                                   \
                if (KeyEqualFunc(*key, *existing_key)) {                                                 \
                    return NK_LITERAL(_##Name##_SearchResult){node, true};                               \
                }                                                                                        \
            } /*fallthrough*/                                                                            \
            case -1:                                                                                     \
                node = (*node)->child + 0;                                                               \
                break;                                                                                   \
            case +1:                                                                                     \
                node = (*node)->child + 1;                                                               \
                break;                                                                                   \
            }                                                                                            \
        }                                                                                                \
        return NK_LITERAL(_##Name##_SearchResult){node, false};                                          \
    }                                                                                                    \
                                                                                                         \
    TItem *Name##_insert(Name *ht, TItem const item) {                                                   \
        TKey const *key = GetKeyFunc(&item);                                                             \
        u64 hash = KeyHashFunc(*key);                                                                    \
        _##Name##_SearchResult res = _##Name##_findNode(&ht->root, key, hash);                           \
        if (!res.existing) {                                                                             \
            NkAllocator _alloc = ht->alloc.proc ? ht->alloc : nk_default_allocator;                      \
            *res.node = (_##Name##_Node *)nk_allocAligned(                                               \
                _alloc, sizeof(_##Name##_Node), nk_maxu(alignof(TItem), alignof(_##Name##_Node)));       \
            **res.node = NK_LITERAL(_##Name##_Node){                                                     \
                .item = item,                                                                            \
                .hash = hash,                                                                            \
                .child = {0},                                                                            \
            };                                                                                           \
        }                                                                                                \
        return &(*res.node)->item;                                                                       \
    }                                                                                                    \
                                                                                                         \
    TItem *Name##_find(Name *ht, TKey const key) {                                                       \
        u64 hash = KeyHashFunc(key);                                                                     \
        _##Name##_SearchResult res = _##Name##_findNode(&ht->root, &key, hash);                          \
        return &(*res.node)->item;                                                                       \
    }                                                                                                    \
                                                                                                         \
    static void _##Name##_freeNode(NkAllocator alloc, _##Name##_Node *node) {                            \
        if (!node) {                                                                                     \
            return;                                                                                      \
        }                                                                                                \
        _##Name##_freeNode(alloc, node->child[0]);                                                       \
        _##Name##_freeNode(alloc, node->child[1]);                                                       \
        nk_free(alloc, node, sizeof(_##Name##_Node));                                                    \
    }                                                                                                    \
                                                                                                         \
    void Name##_free(Name *ht) {                                                                         \
        NkAllocator _alloc = ht->alloc.proc ? ht->alloc : nk_default_allocator;                          \
        _##Name##_freeNode(_alloc, ht->root);                                                            \
        ht->root = NULL;                                                                                 \
    }                                                                                                    \
                                                                                                         \
    _NK_NOP_TOPLEVEL

#define NK_HASH_TREE_DEFINE(Name, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc) \
    NK_HASH_TREE_TYPEDEF(Name, TItem);                                                \
    NK_HASH_TREE_PROTO(Name, TItem, TKey);                                            \
    NK_HASH_TREE_IMPL(Name, TItem, TKey, GetKeyFunc, KeyHashFunc, KeyEqualFunc)

#endif // NTK_HASH_TREE_H_
