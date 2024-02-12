#ifndef NKIRC_PARSER_H_
#define NKIRC_PARSER_H_

#include "irc.h"
#include "nkl/common/token.h"
#include "ntk/atom.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_parse(NkIrCompiler c, NkAtom file, NkString text, NklTokenArray tokens);

#ifdef __cplusplus
}
#endif

#endif // NKIRC_PARSER_H_
