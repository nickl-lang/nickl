#ifndef NK_VM_TRANSLATE_TO_C_H_
#define NK_VM_TRANSLATE_TO_C_H_

#include "nk/vm/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_translateToC(NkIrProg ir, NkIrFunct entry_point, NkStream src);

#ifdef __cplusplus
}
#endif

#endif // NK_VM_TRANSLATE_TO_C_H_
