#ifndef NKL_CORE_PARSER_H_
#define NKL_CORE_PARSER_H_

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkString text;
    NklTokenArray tokens;
    NkArena *arena;

    NkString *err_str;
    NklToken *err_token;

    char const **token_names;
} NklAstParserData;

bool nkl_ast_parse(NklAstParserData const *data, NklAstNodeArray *out_nodes);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_PARSER_H_
