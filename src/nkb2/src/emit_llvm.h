#ifndef NKB_EMIT_LLVM_H_
#define NKB_EMIT_LLVM_H_

#include "nkb/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_emit_llvm(NkStream out, NkArena *scratch, NkIrSymbolArray mod);

#ifdef __cplusplus
}
#endif

#endif // NKB_EMIT_LLVM_H_
