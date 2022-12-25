#ifndef HEADER_GUARD_NK_VM_BYTECODE
#define HEADER_GUARD_NK_VM_BYTECODE

#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkBcProg_T *NkBcProg;

NkBcProg nkbc_createProgram(NkIrProg ir);
void nkbc_deinitProgram(NkBcProg p);

void nkbc_invoke(NkBcProg p, NkIrFunct fn, nkval_t ret, nkval_t args);

typedef struct NkBcFunct_T *NkBcFunct;

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_BYTECODE
