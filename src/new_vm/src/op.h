#ifndef HEADER_GUARD_NK_VM_OP
#define HEADER_GUARD_NK_VM_OP

#include "nk/common/utils.h"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkOpProg_T *NkOpProg;

NkOpProg nkop_createProgram(NkIrProg ir);
void nkop_deinitProgram(NkOpProg p);

void nkop_invoke(NkOpProg p, NkIrFunctId fn, nkval_t ret, nkval_t args);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_OP
