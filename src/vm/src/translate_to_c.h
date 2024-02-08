#ifndef HEADER_GUARD_NK_VM_TRANSLATE_TO_C
#define HEADER_GUARD_NK_VM_TRANSLATE_TO_C

#include "nk/vm/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_translateToC(NkIrProg ir, NkIrFunct entry_point, nk_stream src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_TRANSLATE_TO_C
