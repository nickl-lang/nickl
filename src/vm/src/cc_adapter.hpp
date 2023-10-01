#ifndef HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
#define HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER

#include "nk/vm/ir_compile.h"
#include "ntk/stream.h"

nk_stream nkcc_streamOpen(NkIrCompilerConfig const &conf);
int nkcc_streamClose(nk_stream stream);

#endif // HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
