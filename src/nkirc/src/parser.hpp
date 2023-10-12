#ifndef HEADER_GUARD_NKB_PARSER
#define HEADER_GUARD_NKB_PARSER

#include "irc.hpp"
#include "lexer.hpp"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/id.h"
#include "ntk/string.h"

typedef struct {
    NkIrProc entry_point;
    nks error_msg;
    bool ok;
} NkIrParserState;

void nkir_parse(NkIrParserState *parser, NkIrCompiler c, nkid file, NkIrTokenView tokens);

#endif // HEADER_GUARD_NKB_PARSER
