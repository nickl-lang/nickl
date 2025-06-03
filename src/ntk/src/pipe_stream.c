#include "ntk/pipe_stream.h"

#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/process.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(pipe_stream);

bool nk_pipe_streamOpenRead(NkPipeStream *ps, NkPipeStreamInfo const info, NkStream *out) {
    NK_LOG_TRC("%s", __func__);

    bool ret = false;
    NK_PROF_FUNC() {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "exec(\"");
            nks_escape(log, info.cmd);
            nk_printf(log, "\")");
        }

        NkPipe out_pipe = nk_pipe_create();
        NkPipe null_pipe = {
            info.quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
            info.quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
        };
        NkHandle process = NK_NULL_HANDLE;
        if (nk_execAsync(info.scratch, info.cmd, &process, NULL, &out_pipe, &null_pipe) < 0) {
            nkerr_t const err = nk_getLastError();

            nk_waitProc(process, NULL);

            nk_pipe_close(out_pipe);
            nk_pipe_close(null_pipe);

            nk_setLastError(err);
        } else {
            *ps = (NkPipeStream){
                ._stream = out,
                ._buf =
                    {
                        .file = out_pipe.read_file,
                        .buf = info.opt_buf,
                    },
                ._process = process,
            };
            *out = nk_file_getBufferedWriteStream(&ps->_buf);
            ret = true;
        }
    }
    return ret;
}

bool nk_pipe_streamOpenWrite(NkPipeStream *ps, NkPipeStreamInfo const info, NkStream *out) {
    NK_LOG_TRC("%s", __func__);

    bool ret = false;
    NK_PROF_FUNC() {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "exec(\"");
            nks_escape(log, info.cmd);
            nk_printf(log, "\")");
        }

        NkPipe in_pipe = nk_pipe_create();
        NkPipe null_pipe = {
            NK_NULL_HANDLE,
            info.quiet ? nk_open(nk_null_file, NkOpenFlags_Write) : NK_NULL_HANDLE,
        };
        NkHandle process = NK_NULL_HANDLE;
        if (nk_execAsync(info.scratch, info.cmd, &process, &in_pipe, &null_pipe, &null_pipe) < 0) {
            nkerr_t const err = nk_getLastError();

            nk_waitProc(process, NULL);

            nk_pipe_close(in_pipe);
            nk_pipe_close(null_pipe);

            nk_setLastError(err);
        } else {
            *ps = (NkPipeStream){
                ._stream = out,
                ._buf =
                    {
                        .file = in_pipe.write_file,
                        .buf = info.opt_buf,
                    },
                ._process = process,
            };
            *out = nk_file_getBufferedWriteStream(&ps->_buf);
            ret = true;
        }
    }
    return ret;
}

i32 nk_pipe_streamClose(NkPipeStream *ps) {
    NK_LOG_TRC("%s", __func__);

    i32 ret = 1;
    NK_PROF_FUNC() {
        nk_stream_flush(*ps->_stream);
        nk_close(ps->_buf.file);
        nk_waitProc(ps->_process, &ret);

        *ps = (NkPipeStream){0};
    }
    return ret;
}
