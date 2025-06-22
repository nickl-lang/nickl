#ifndef NKB_LLVM_EMITTER_H_
#define NKB_LLVM_EMITTER_H_

#include "nkb/ir.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_llvm_emitIr(NkStream out, NkArena *scratch, NkIrSymbolArray mod);

#ifdef __cplusplus
}
#endif

#endif // NKB_LLVM_EMITTER_H_
