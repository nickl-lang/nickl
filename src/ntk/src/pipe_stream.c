#include "ntk/pipe_stream.h"

#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/process.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(pipe_stream);

#define CMD_BUF_SIZE 4096

bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, NkString cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    bool ret = false;
    NK_PROF_FUNC() {
        NKSB_FIXED_BUFFER(sb, CMD_BUF_SIZE);
        nksb_tryAppendStr(&sb, cmd);
        nksb_tryAppendNull(&sb);

        NK_LOG_DBG("exec(\"" NKS_FMT "\")", NKS_ARG(sb));

        NkPipe out = nk_pipe_create();
        NkPipe null_pipe = {
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
        };
        NkHandle process = NK_NULL_HANDLE;
        if (nk_execAsync(sb.data, &process, NULL, &out, &null_pipe) < 0) {
            nkerr_t err = nk_getLastError();

            nk_waitProc(process, NULL);

            nk_pipe_close(out);
            nk_pipe_close(null_pipe);

            nk_setLastError(err);
        } else {
            *pipe_stream = (NkPipeStream){
                .stream = nk_file_getStream(out.read_file),
                ._file = out.read_file,
                ._process = process,
            };
            ret = true;
        }
    }
    return ret;
}

bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, NkString cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    bool ret = false;
    NK_PROF_FUNC() {
        NKSB_FIXED_BUFFER(sb, CMD_BUF_SIZE);
        nksb_tryAppendStr(&sb, cmd);
        nksb_tryAppendNull(&sb);

        NK_LOG_DBG("exec(\"" NKS_FMT "\")", NKS_ARG(sb));
        NkPipe in = nk_pipe_create();
        NkPipe null_pipe = {
            NK_NULL_HANDLE,
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
        };
        NkHandle process = NK_NULL_HANDLE;
        if (nk_execAsync(sb.data, &process, &in, &null_pipe, &null_pipe) < 0) {
            nkerr_t err = nk_getLastError();

            nk_waitProc(process, NULL);

            nk_pipe_close(in);
            nk_pipe_close(null_pipe);

            nk_setLastError(err);
        } else {
            *pipe_stream = (NkPipeStream){
                .stream = nk_file_getStream(in.write_file),
                ._file = in.write_file,
                ._process = process,
            };
            ret = true;
        }
    }
    return ret;
}

i32 nk_pipe_streamClose(NkPipeStream *pipe_stream) {
    NK_LOG_TRC("%s", __func__);

    i32 ret = 1;
    NK_PROF_FUNC() {
        nk_close(pipe_stream->_file);
        nk_waitProc(pipe_stream->_process, &ret);

        *pipe_stream = (NkPipeStream){0};
    }
    return ret;
}
