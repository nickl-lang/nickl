#ifndef HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
#define HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER

#include "nk/vm/ir_compile.h"
#include "ntk/pipe_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

NkPipeStream nkcc_streamOpen(NkIrCompilerConfig const conf);
int nkcc_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
