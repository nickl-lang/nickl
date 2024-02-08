#ifndef HEADER_GUARD_NKB_CC_ADAPTER
#define HEADER_GUARD_NKB_CC_ADAPTER

#include "nkb/ir.h"
#include "ntk/pipe_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nkcc_streamOpen(NkPipeStream *stream, NkIrCompilerConfig conf);
int nkcc_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_CC_ADAPTER
