#ifndef HEADER_GUARD_NKL_COMMON_AST
#define HEADER_GUARD_NKL_COMMON_AST

#include <stdint.h>

#include "nkl/common/token.h"
#include "ntk/array.h"
#include "ntk/id.h"
#include "ntk/stream.h"

#ifndef __cplusplus
extern "C" {
#endif

typedef struct {
    nkid id;
    uint32_t token_idx;
    uint32_t total_children;
    uint32_t arity;
} NklAstNode;

nkav_typedef(NklAstNode, NklAstNodeView);
nkar_typedef(NklAstNode, NklAstNodeArray);

void nkl_ast_inspect(NklAstNodeView nodes, NklTokenView tokens, nk_stream out);

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_AST
