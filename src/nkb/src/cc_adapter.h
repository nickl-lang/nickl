#ifndef HEADER_GUARD_NKB_CC_ADAPTER
#define HEADER_GUARD_NKB_CC_ADAPTER

#include "nkb/ir.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nkcc_streamOpen(nk_stream *stream, NkIrCompilerConfig conf);
int nkcc_streamClose(nk_stream stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_CC_ADAPTER
