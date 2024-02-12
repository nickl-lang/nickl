#ifndef NKB_TRANSLATE2C_H_
#define NKB_TRANSLATE2C_H_

#include "nkb/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_translate2c(NkArena *arena, NkIrProg ir, NkStream src);

#ifdef __cplusplus
}
#endif

#endif // NKB_TRANSLATE2C_H_
