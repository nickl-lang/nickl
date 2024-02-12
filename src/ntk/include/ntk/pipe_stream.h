#ifndef NTK_PIPE_STREAM_H_
#define NTK_PIPE_STREAM_H_

#include "ntk/os/process.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkStream stream;
    nkfd_t fd;
    nkpid_t pid;
} NkPipeStream;

bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, NkString cmd, bool quiet);
bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, NkString cmd, bool quiet);

i32 nk_pipe_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // NTK_PIPE_STREAM_H_
