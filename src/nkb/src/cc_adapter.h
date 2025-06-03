#ifndef NKB_CC_ADAPTER_H_
#define NKB_CC_ADAPTER_H_

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/pipe_stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nkcc_streamOpen(
    NkArena *scratch,
    NkPipeStream *ps,
    NkStringBuf opt_buf,
    NkIrCompilerConfig const conf,
    NkStream *out);
int nkcc_streamClose(NkPipeStream *ps);

#ifdef __cplusplus
}
#endif

#endif // NKB_CC_ADAPTER_H_
