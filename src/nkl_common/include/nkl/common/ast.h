#ifndef NKL_COMMON_AST_H_
#define NKL_COMMON_AST_H_

#include "nkl/common/token.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkAtom id;
    u32 token_idx;
    u32 total_children;
    u32 arity;
} NklAstNode;

typedef NkSlice(NklAstNode const) NklAstNodeArray;
typedef NkDynArray(NklAstNode) NklAstNodeDynArray;

typedef struct {
    NkAtom file; // TODO: Move it, it's unused in ast
    NkString text;
    NklTokenArray tokens;
    NklAstNodeArray nodes;
} NklSource;

void nkl_ast_inspect(NklSource src, NkStream out);

NK_INLINE u32 nkl_ast_nextChild(NklAstNodeArray nodes, u32 idx) {
    nk_assert(idx < nodes.size && "node index out of range");
    return idx + nodes.data[idx].total_children + 1;
}

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_AST_H_
