#ifndef HEADER_GUARD_NKB_TRANSLATE2C
#define HEADER_GUARD_NKB_TRANSLATE2C

#include "nkb/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_translate2c(NkIrProg ir, NkIrProc entry_point, nk_stream src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_TRANSLATE2C
