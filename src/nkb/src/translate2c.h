#ifndef HEADER_GUARD_NKB_TRANSLATE2C
#define HEADER_GUARD_NKB_TRANSLATE2C

#include "nkb/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_translate2c(NkArena *arena, NkIrProg ir, nk_stream src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_TRANSLATE2C
