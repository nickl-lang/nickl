#ifndef HEADER_GUARD_NKB_PARSER
#define HEADER_GUARD_NKB_PARSER

#include "lexer.hpp"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/string.h"
#include "types.hpp"

typedef struct {
    NkIrProg ir;
    NkIrProc entry_point;
    nks error_msg;
    bool ok;
} NkIrParserState;

void nkir_parse(
    NkIrParserState *parser,
    NkIrTypeCache *types,
    NkArena *file_arena,
    NkArena *tmp_arena,
    NkIrTokenView tokens);

#endif // HEADER_GUARD_NKB_PARSER
