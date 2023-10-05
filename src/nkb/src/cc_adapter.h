#ifndef HEADER_GUARD_NKB_CC_ADAPTER
#define HEADER_GUARD_NKB_CC_ADAPTER

#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nks compiler_binary;
    nks additional_flags;
    nks output_filename;
    bool quiet;
} NkIrCompilerConfig;

bool nkcc_streamOpen(nk_stream *stream, NkIrCompilerConfig conf);
int nkcc_streamClose(nk_stream stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_CC_ADAPTER
