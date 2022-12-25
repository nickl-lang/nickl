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

typedef struct NklNode NklNode;

typedef struct {
    NklNode const *data;
    size_t size;
} NklNodeArray;

struct NklNode {
    NklNodeArray args[3];
    NklToken const *token;
    nkid id;
};

typedef struct NklAst_T *NklAst;

NklAst nkl_ast_create();
void nkl_ast_free(NklAst ast);

NklNode const *nkl_ast_pushNode(NklAst ast, NklNode node);
NklNodeArray nkl_ast_pushNodeAr(NklAst ast, NklNodeArray ar);

void nkl_ast_inspect(NklNode const *root, NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_AST
