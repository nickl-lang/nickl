#ifndef HEADER_GUARD_NKL_LANG_AST
#define HEADER_GUARD_NKL_LANG_AST

#include "nk/common/id.h"
#include "nk/common/string_builder.h"
#include "nkl/lang/token.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char const *s_nkl_ast_node_names[];

typedef struct NklAstNode_T const *NklAstNode;

typedef struct {
    NklAstNode data;
    size_t size;
} NklAstNodeArray;

struct NklAstNode_T {
    NklAstNodeArray args[3];
    NklTokenRef token;
    nkid id;
};

typedef struct NklAst_T *NklAst;

NklAst nkl_ast_create();
void nkl_ast_free(NklAst ast);

NklAstNode_T nkl_makeNode0(char const *id, NklTokenRef token);
NklAstNode_T nkl_makeNode1(char const *id, NklTokenRef token, NklAstNodeArray arg0);
NklAstNode_T nkl_makeNode2(
    char const *id,
    NklTokenRef token,
    NklAstNodeArray arg0,
    NklAstNodeArray arg1);
NklAstNode_T nkl_makeNode3(
    char const *id,
    NklTokenRef token,
    NklAstNodeArray arg0,
    NklAstNodeArray arg1,
    NklAstNodeArray arg2);

NklAstNodeArray nkl_pushNode(NklAst ast, NklAstNode_T node);
NklAstNodeArray nkl_pushNodeAr(NklAst ast, NklAstNodeArray ar);

void nkl_inspectNode(NklAstNode root, NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_AST
