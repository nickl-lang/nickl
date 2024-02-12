#ifndef NKSTC_PARSER_H_
#define NKSTC_PARSER_H_

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NklAstNodeDynArray nodes;
    NkString error_msg;
    NklToken error_token;
} NkStParserState;

bool nkst_parse(NkStParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NkString text, NklTokenArray tokens);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_PARSER_H_
