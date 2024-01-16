#ifndef HEADER_GUARD_NKSTC_AST_IMPL
#define HEADER_GUARD_NKSTC_AST_IMPL

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

#endif // HEADER_GUARD_NKSTC_AST_IMPL
