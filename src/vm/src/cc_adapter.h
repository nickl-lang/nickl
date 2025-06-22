#ifndef NK_VM_C_COMPILER_ADAPTER_H_
#define NK_VM_C_COMPILER_ADAPTER_H_

#include "nk/vm/ir_compile.h"
#include "ntk/arena.h"
#include "ntk/pipe_stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkcc_streamOpen(NkArena *scratch, NkPipeStream *ps, NkStringBuf opt_buf, NkIrCompilerConfig conf, NkStream *out);
int nkcc_streamClose(NkPipeStream *ps);

#ifdef __cplusplus
}
#endif

#endif // NK_VM_C_COMPILER_ADAPTER_H_
