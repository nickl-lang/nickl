#ifndef HEADER_GUARD_NKB_PARSER
#define HEADER_GUARD_NKB_PARSER

#include "lexer.hpp"
#include "nk/common/allocator.h"
#include "nk/common/slice.hpp"
#include "nk/common/string.h"

typedef struct {
    nkstr error_msg;
    bool ok;
} NkIrParserState;

void nkir_parse(NkIrParserState *parser, NkAllocator tmp_alloc, NkSlice<NkIrToken> tokens);

#endif // HEADER_GUARD_NKB_PARSER
