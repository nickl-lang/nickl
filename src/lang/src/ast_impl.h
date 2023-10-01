#ifndef HEADER_GUARD_NKL_LANG_AST_IMPL
#define HEADER_GUARD_NKL_LANG_AST_IMPL

#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define X(ID) CAT(n_, ID),
#include "nodes.inl"

    node_count,
} NklAstNodeId;

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_AST_IMPL
