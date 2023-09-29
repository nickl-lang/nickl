#ifndef HEADER_GUARD_NK_VM_PIPE_STREAM
#define HEADER_GUARD_NK_VM_PIPE_STREAM

#include "nk/common/stream.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

nk_stream nk_pipe_streamRead(nkstr cmd, bool quiet);
nk_stream nk_pipe_streamWrite(nkstr cmd, bool quiet);

int nk_pipe_streamClose(nk_stream stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_PIPE_STREAM
