#ifndef HEADER_GUARD_NTK_PIPE_STREAM
#define HEADER_GUARD_NTK_PIPE_STREAM

#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/sys/process.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nk_stream stream;
    nkfd_t fd;
    nkpid_t pid;
} NkPipeStream;

bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, nks cmd, bool quiet);
bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, nks cmd, bool quiet);

i32 nk_pipe_streamClose(NkPipeStream *stream);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_PIPE_STREAM
