#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include <stddef.h>

#include "nk/common/id.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkstr text;

    size_t pos;
    size_t lin;
    size_t col;

    size_t id;
} NklToken; // TODO Move NklToken declaration

typedef struct NklAstNode_T const *NklAstNode;

typedef struct {
    NklAstNode data;
    size_t size;
} NklAstNodeArray;

struct NklAstNode_T {
    NklAstNodeArray args[3];
    NklToken const *token;
    nkid id;
};

typedef struct NklAst_T *NklAst;

NklAst nkl_ast_create();
void nkl_ast_free(NklAst ast);

NklAstNode nkl_ast_pushNode(NklAst ast, NklAstNode node);
NklAstNodeArray nkl_ast_pushNodeAr(NklAst ast, NklAstNodeArray ar);

void nkl_ast_inspect(NklAstNode root, NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_AST
