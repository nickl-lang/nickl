#ifndef HEADER_GUARD_NKSTC_PARSER
#define HEADER_GUARD_NKSTC_PARSER

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/id.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NklAstNodeArray nodes;
    nks error_msg;
    NklToken error_token;
} NkStParserState;

bool nkst_parse(NkStParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NklTokenView tokens);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKSTC_PARSER
