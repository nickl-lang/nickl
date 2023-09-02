#ifndef HEADER_GUARD_NKB_PARSER
#define HEADER_GUARD_NKB_PARSER

#include "lexer.hpp"
#include "nk/common/allocator.h"
#include "nk/common/slice.hpp"
#include "nk/common/string.h"
#include "nkb/ir.h"

typedef struct {
    NkIrProg ir;
    NkIrProc entry_point;
    nkstr error_msg;
    bool ok;
} NkIrParserState;

void nkir_parse(NkIrParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NkSlice<NkIrToken> tokens);

#endif // HEADER_GUARD_NKB_PARSER
