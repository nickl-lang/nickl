#ifndef NKL_CORE_IR_PARSER_H_
#define NKL_CORE_IR_PARSER_H_

#include "nkb/ir.h"
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
} NklIrParserData;

bool nkl_ir_parse(NklIrParserData const *data, NkIrSymbolDynArray *out_syms);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_IR_PARSER_H_
