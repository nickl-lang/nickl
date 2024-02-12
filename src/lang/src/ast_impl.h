#ifndef NKL_LANG_AST_IMPL_H_
#define NKL_LANG_AST_IMPL_H_

#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define X(ID) NK_CAT(n_, ID),
#include "nodes.inl"

    node_count,
} NklAstNodeId;

#ifdef __cplusplus
}
#endif

#endif // NKL_LANG_AST_IMPL_H_
