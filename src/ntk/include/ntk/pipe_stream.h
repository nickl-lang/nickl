#ifndef HEADER_GUARD_NTK_PIPE_STREAM
#define HEADER_GUARD_NTK_PIPE_STREAM

#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

nk_stream nk_pipe_streamRead(nkstr cmd, bool quiet);
nk_stream nk_pipe_streamWrite(nkstr cmd, bool quiet);

int nk_pipe_streamClose(nk_stream stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_PIPE_STREAM
