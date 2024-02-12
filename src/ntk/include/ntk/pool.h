#ifndef NTK_POOL_H_
#define NTK_POOL_H_

#include "ntk/dyn_array.h"

#define NK_POOL_TYPEDEF(TPool, TItem)               \
    typedef struct _##TPool##_Node _##TPool##_Node; \
    struct _##TPool##_Node {                        \
        TItem item;                                 \
        _##TPool##_Node *next;                      \
    };                                              \
    typedef struct {                                \
        NkDynArray(_##TPool##_Node) items;          \
        _##TPool##_Node *next;                      \
    } TPool

#define NK_POOL_INIT(alloc) .items{NKDA_INIT(alloc)}, .next = NULL

#define NK_POOL_PROTO(TPool, TItem)                 \
    TItem *TPool##_alloc(TPool *pool);              \
    void TPool##_release(TPool *pool, TItem *item); \
    void TPool##_free(TPool *pool)

#define NK_POOL_IMPL(TPool, TItem)                                                 \
    TItem *TPool##_alloc(TPool *pool) {                                            \
        if (pool->next) {                                                          \
            _##TPool##_Node *node = pool->next;                                    \
            pool->next = node->next;                                               \
            *node = NK_LITERAL(_##TPool##_Node) NK_ZERO_STRUCT;                    \
            return &node->item;                                                    \
        } else {                                                                   \
            nkda_append(&pool->items, NK_LITERAL(_##TPool##_Node) NK_ZERO_STRUCT); \
            return &nk_slice_last(pool->items).item;                               \
        }                                                                          \
    }                                                                              \
                                                                                   \
    void TPool##_release(TPool *pool, TItem *item) {                               \
        _##TPool##_Node *node = (_##TPool##_Node *)item;                           \
        node->next = pool->next;                                                   \
        pool->next = node;                                                         \
    }                                                                              \
                                                                                   \
    void TPool##_free(TPool *pool) {                                               \
        nkda_free(&pool->items);                                                   \
        pool->next = NULL;                                                         \
    }                                                                              \
                                                                                   \
    _NK_NOP_TOPLEVEL

#define NK_POOL_DEFINE(TPool, TItem) \
    NK_POOL_TYPEDEF(TPool, TItem);   \
    NK_POOL_PROTO(TPool, TItem);     \
    NK_POOL_IMPL(TPool, TItem)

#endif // NTK_POOL_H_
