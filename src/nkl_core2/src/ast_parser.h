#ifndef NKL_CORE_AST_PARSER_H_
#define NKL_CORE_AST_PARSER_H_

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NklState nkl;
    NkAtom file;

    NkString *err_str;
    NklToken *err_token;
    char const **token_names;
} NklAstParserData;

bool nkl_ast_parse(NklAstParserData const *data, NklAstNodeArray *out_nodes);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_AST_PARSER_H_
