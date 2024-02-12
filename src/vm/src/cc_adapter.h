#ifndef NK_VM_C_COMPILER_ADAPTER_H_
#define NK_VM_C_COMPILER_ADAPTER_H_

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

#endif // NK_VM_C_COMPILER_ADAPTER_H_
