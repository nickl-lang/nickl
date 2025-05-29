#ifndef NKB_CC_ADAPTER_H_
#define NKB_CC_ADAPTER_H_

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/pipe_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nkcc_streamOpen(NkArena *scratch, NkPipeStream *stream, NkIrCompilerConfig conf);
int nkcc_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // NKB_CC_ADAPTER_H_
