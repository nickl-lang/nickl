#ifndef NK_VM_BYTECODE_H_
#define NK_VM_BYTECODE_H_

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

#endif // NK_VM_BYTECODE_H_
