#ifndef NKSTC_AST_IMPL_H_
#define NKSTC_AST_IMPL_H_

#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define XN(N, T) NK_CAT(n_, N),
#include "nodes.inl"

    node_count,
} NklAstNodeId;

#ifdef __cplusplus
}
#endif

#endif // NKSTC_AST_IMPL_H_
