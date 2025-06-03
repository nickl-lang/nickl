#ifndef NTK_PIPE_STREAM_H_
#define NTK_PIPE_STREAM_H_

#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkStream *_stream;
    NkFileStreamBuf _buf;
    NkHandle _process;
} NkPipeStream;

typedef struct {
    NkPipeStream *ps;
    NkArena *scratch;
    NkString cmd;
    NkStringBuf opt_buf;
    bool quiet;
} NkPipeStreamInfo;

NK_EXPORT bool nk_pipe_streamOpenRead(NkPipeStreamInfo const info, NkStream *out);
NK_EXPORT bool nk_pipe_streamOpenWrite(NkPipeStreamInfo const info, NkStream *out);

NK_EXPORT i32 nk_pipe_streamClose(NkPipeStream *ps);

#ifdef __cplusplus
}
#endif

#endif // NTK_PIPE_STREAM_H_
