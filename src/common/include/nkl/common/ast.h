#ifndef NKL_COMMON_AST_H_
#define NKL_COMMON_AST_H_

#include "nkl/common/token.h"
#include "ntk/atom.h"
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

typedef NkSlice(NklAstNode) NklAstNodeArray;
typedef NkDynArray(NklAstNode) NklAstNodeDynArray;

typedef struct {
    NkString text;
    NklTokenArray tokens;
    NklAstNodeArray nodes;
} NklSource;

void nkl_ast_inspect(NklSource src, NkStream out);

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_AST_H_
