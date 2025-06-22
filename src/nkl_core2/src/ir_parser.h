#ifndef NKL_CORE_IR_PARSER_H_
#define NKL_CORE_IR_PARSER_H_

#include "nkl/core/nickl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NklModule mod;
    NkAtom file;
    char const **token_names;
} NklIrParserData;

bool nkl_ir_parse(NklIrParserData const *data);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_IR_PARSER_H_
