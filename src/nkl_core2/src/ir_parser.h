#ifndef NKL_CORE_IR_PARSER_H_
#define NKL_CORE_IR_PARSER_H_

#include "nkb/ir.h"
#include "nkl/core/nickl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NklState nkl;
    NkAtom file;
    char const **token_names;
} NklIrParserData;

bool nkl_ir_parse(NklIrParserData const *data, NkIrSymbolDynArray *out_syms);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_IR_PARSER_H_
