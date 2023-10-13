#ifndef HEADER_GUARD_NKIRC_PARSER
#define HEADER_GUARD_NKIRC_PARSER

#include "irc.hpp"
#include "lexer.hpp"
#include "ntk/id.h"

void nkir_parse(NkIrCompiler c, nkid file, NkIrTokenView tokens);

#endif // HEADER_GUARD_NKIRC_PARSER
