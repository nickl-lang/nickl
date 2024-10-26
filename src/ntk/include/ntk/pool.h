#ifndef NTK_POOL_H_
#define NTK_POOL_H_

#include "ntk/arena.h"

#define NK_POOL_TYPEDEF(TPool, TItem)               \
    typedef struct _##TPool##_Node _##TPool##_Node; \
    struct _##TPool##_Node {                        \
        TItem item;                                 \
        _##TPool##_Node *next;                      \
    };                                              \
    typedef struct {                                \
        NkArena arena;                              \
        _##TPool##_Node *next;                      \
    } TPool

#define NK_POOL_PROTO(TPool, TItem)                 \
    TItem *TPool##_alloc(TPool *pool);              \
    void TPool##_release(TPool *pool, TItem *item); \
    void TPool##_free(TPool *pool);                 \
    NK_POOL_SHARED_PROTO_(TPool, TItem)

#define NK_POOL_IMPL(TPool, TItem)                                                           \
    TItem *TPool##_alloc(TPool *pool) {                                                      \
        _##TPool##_Node *node;                                                               \
        if (pool->next) {                                                                    \
            node = pool->next;                                                               \
            pool->next = node->next;                                                         \
        } else {                                                                             \
            node = (_##TPool##_Node *)nk_arena_alloc(&pool->arena, sizeof(_##TPool##_Node)); \
        }                                                                                    \
        *node = NK_LITERAL(_##TPool##_Node) NK_ZERO_STRUCT;                                  \
        return &node->item;                                                                  \
    }                                                                                        \
                                                                                             \
    void TPool##_release(TPool *pool, TItem *item) {                                         \
        _##TPool##_Node *node = (_##TPool##_Node *)item;                                     \
        node->next = pool->next;                                                             \
        pool->next = node;                                                                   \
    }                                                                                        \
                                                                                             \
    void TPool##_free(TPool *pool) {                                                         \
        nk_arena_free(&pool->arena);                                                         \
        pool->next = NULL;                                                                   \
    }                                                                                        \
                                                                                             \
    NK_POOL_SHARED_IMPL_(TPool, TItem)

#define NK_POOL_DEFINE(TPool, TItem) \
    NK_POOL_TYPEDEF(TPool, TItem);   \
    NK_POOL_PROTO(TPool, TItem);     \
    NK_POOL_IMPL(TPool, TItem)

#ifdef __cplusplus

#include <memory>

#define NK_POOL_SHARED_PROTO_(TPool, TItem) std::shared_ptr<TItem> TPool##_allocShared(TPool *pool)

#define NK_POOL_SHARED_IMPL_(TPool, TItem)                                       \
    std::shared_ptr<TItem> TPool##_allocShared(TPool *pool) {                    \
        return std::shared_ptr<TItem>{TPool##_alloc(pool), [pool](TItem *item) { \
                                          TPool##_release(pool, item);           \
                                      }};                                        \
    }                                                                            \
                                                                                 \
    _NK_NOP_TOPLEVEL

#else // __cplusplus

#define NK_POOL_SHARED_PROTO_(TPool, TItem) _NK_NOP_TOPLEVEL
#define NK_POOL_SHARED_IMPL_(TPool, TItem) _NK_NOP_TOPLEVEL

#endif //__cplusplus

#endif // NTK_POOL_H_
