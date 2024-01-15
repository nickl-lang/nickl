#ifndef HEADER_GUARD_NKIRC_PARSER
#define HEADER_GUARD_NKIRC_PARSER

#include "irc.h"
#include "lexer.h"
#include "ntk/id.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_parse(NkIrCompiler c, nkid file, nks text, NklTokenView tokens);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_PARSER
