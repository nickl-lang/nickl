#ifndef HEADER_GUARD_NKL_COMMON_AST
#define HEADER_GUARD_NKL_COMMON_AST

#include <stdint.h>

#include "nkl/common/token.h"
#include "ntk/array.h"
#include "ntk/id.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkid id;
    u32 token_idx;
    u32 total_children;
    u32 arity;
} NklAstNode;

nkav_typedef(NklAstNode, NklAstNodeView);
nkar_typedef(NklAstNode, NklAstNodeArray);

typedef struct {
    nks text;
    NklTokenView tokens;
    NklAstNodeView nodes;
} NklSource;

void nkl_ast_inspect(NklSource src, nk_stream out);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_AST
