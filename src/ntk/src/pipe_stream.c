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

        NkPipe out = nk_proc_createPipe();
        NkPipe null_pipe = {
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_HANDLE_ZERO,
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_HANDLE_ZERO,
        };
        NkHandle h_process = NK_HANDLE_ZERO;
        if (nk_proc_execAsync(sb.data, &h_process, NULL, &out, &null_pipe) < 0) {
            nkerr_t err = nk_getLastError();

            nk_proc_wait(h_process, NULL);

            nk_proc_closePipe(out);
            nk_proc_closePipe(null_pipe);

            nk_setLastError(err);
        } else {
            *pipe_stream = (NkPipeStream){
                nk_file_getStream(out.h_read),
                out.h_read,
                h_process,
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
        NkPipe in = nk_proc_createPipe();
        NkPipe null_pipe = {
            NK_HANDLE_ZERO,
            quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_HANDLE_ZERO,
        };
        NkHandle h_process = NK_HANDLE_ZERO;
        if (nk_proc_execAsync(sb.data, &h_process, &in, &null_pipe, &null_pipe) < 0) {
            nkerr_t err = nk_getLastError();

            nk_proc_wait(h_process, NULL);

            nk_proc_closePipe(in);
            nk_proc_closePipe(null_pipe);

            nk_setLastError(err);
        } else {
            *pipe_stream = (NkPipeStream){
                nk_file_getStream(in.h_write),
                in.h_write,
                h_process,
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
        nk_close(pipe_stream->h_file);
        nk_proc_wait(pipe_stream->h_process, &ret);

        *pipe_stream = (NkPipeStream){0};
    }
    return ret;
}
