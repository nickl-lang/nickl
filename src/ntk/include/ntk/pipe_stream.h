#ifndef NTK_PIPE_STREAM_H_
#define NTK_PIPE_STREAM_H_

#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkStream stream;
    NkOsHandle h_file;
    NkOsHandle h_process;
} NkPipeStream;

NK_EXPORT bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, NkString cmd, bool quiet);
NK_EXPORT bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, NkString cmd, bool quiet);

NK_EXPORT i32 nk_pipe_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // NTK_PIPE_STREAM_H_
