#ifndef NKL_LANG_AST_H_
#define NKL_LANG_AST_H_

#include "nkl/lang/token.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/string_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char const *s_nkl_ast_node_names[];

typedef struct NklAstNode_T const *NklAstNode;

typedef struct {
    NklAstNode data;
    usize size;
} NklAstNodeArray;

struct NklAstNode_T {
    NklAstNodeArray args[3];
    NklTokenRef token;
    NkAtom id;
};

typedef struct NklAst_T *NklAst;

void nkl_ast_init();

NklAst nkl_ast_create();
void nkl_ast_free(NklAst ast);

NklAstNode_T nkl_makeNode0(NkAtom id, NklTokenRef token);
NklAstNode_T nkl_makeNode1(NkAtom id, NklTokenRef token, NklAstNodeArray arg0);
NklAstNode_T nkl_makeNode2(NkAtom id, NklTokenRef token, NklAstNodeArray arg0, NklAstNodeArray arg1);
NklAstNode_T nkl_makeNode3(
    NkAtom id,
    NklTokenRef token,
    NklAstNodeArray arg0,
    NklAstNodeArray arg1,
    NklAstNodeArray arg2);

NklAstNodeArray nkl_pushNode(NklAst ast, NklAstNode_T node);
NklAstNodeArray nkl_pushNodeAr(NklAst ast, NklAstNodeArray ar);

void nkl_inspectNode(NklAstNode root, NkStringBuilder *sb);

#define nargs0(NODE) ((NODE)->args[0])
#define nargs1(NODE) ((NODE)->args[1])
#define nargs2(NODE) ((NODE)->args[2])

#define narg0(NODE) (nargs0(NODE).data)
#define narg1(NODE) (nargs1(NODE).data)
#define narg2(NODE) (nargs2(NODE).data)

#ifdef __cplusplus
}
#endif

#endif // NKL_LANG_AST_H_
