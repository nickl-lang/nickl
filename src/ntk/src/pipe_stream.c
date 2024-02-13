#include "ntk/pipe_stream.h"

#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/os/error.h"
#include "ntk/os/file.h"
#include "ntk/os/process.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(pipe_stream);

#define CMD_BUF_SIZE 4096

bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, NkString cmd, bool quiet) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    NKSB_FIXED_BUFFER(sb, CMD_BUF_SIZE);
    nksb_tryAppendStr(&sb, cmd);
    nksb_tryAppendNull(&sb);

    NK_LOG_DBG("exec(\"" NKS_FMT "\")", NKS_ARG(sb));

    NkPipe out = nk_proc_createPipe();
    NkPipe null_pipe = {
        quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_OS_HANDLE_ZERO,
        quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_OS_HANDLE_ZERO,
    };
    NkOsHandle h_process = NK_OS_HANDLE_ZERO;
    if (nk_proc_execAsync(sb.data, &h_process, NULL, &out, &null_pipe) < 0) {
        nkerr_t err = nk_getLastError();

        nk_proc_wait(h_process, NULL);

        nk_proc_closePipe(out);
        nk_proc_closePipe(null_pipe);

        nk_setLastError(err);

        NK_PROF_FUNC_END();
        return false;
    }

    *pipe_stream = (NkPipeStream){
        nk_file_getStream(out.h_read),
        out.h_read,
        h_process,
    };

    NK_PROF_FUNC_END();
    return true;
}

bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, NkString cmd, bool quiet) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    NKSB_FIXED_BUFFER(sb, CMD_BUF_SIZE);
    nksb_tryAppendStr(&sb, cmd);
    nksb_tryAppendNull(&sb);

    NK_LOG_DBG("exec(\"" NKS_FMT "\")", NKS_ARG(sb));

    NkPipe in = nk_proc_createPipe();
    NkPipe null_pipe = {
        NK_OS_HANDLE_ZERO,
        quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_OS_HANDLE_ZERO,
    };
    NkOsHandle h_process = NK_OS_HANDLE_ZERO;
    if (nk_proc_execAsync(sb.data, &h_process, &in, &null_pipe, &null_pipe) < 0) {
        nkerr_t err = nk_getLastError();

        nk_proc_wait(h_process, NULL);

        nk_proc_closePipe(in);
        nk_proc_closePipe(null_pipe);

        nk_setLastError(err);

        NK_PROF_FUNC_END();
        return false;
    }

    *pipe_stream = (NkPipeStream){
        nk_file_getStream(in.h_write),
        in.h_write,
        h_process,
    };

    NK_PROF_FUNC_END();
    return true;
}

i32 nk_pipe_streamClose(NkPipeStream *pipe_stream) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    i32 ret = 1;
    nk_close(pipe_stream->h_file);
    nk_proc_wait(pipe_stream->h_process, &ret);

    *pipe_stream = (NkPipeStream){0};

    NK_PROF_FUNC_END();
    return ret;
}
