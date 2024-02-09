#include "ntk/pipe_stream.h"

#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string_builder.h"
#include "ntk/sys/error.h"
#include "ntk/sys/file.h"
#include "ntk/sys/process.h"

NK_LOG_USE_SCOPE(pipe_stream);

#define CMD_BUF_SIZE 4096

bool nk_pipe_streamOpenRead(NkPipeStream *pipe_stream, nks cmd, bool quiet) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    nksb_try_append_str(&sb, cmd);
    nksb_try_append_null(&sb);

    NK_LOG_DBG("exec(\"" nks_Fmt "\")", nks_Arg(sb));

    nkpipe_t out = nk_createPipe();
    nkpipe_t null_pipe = {
        quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd,
        quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd};
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, NULL, &out, &null_pipe) < 0) {
        nkerr_t err = nk_getLastError();

        if (pid > 0) {
            nk_waitpid(pid, NULL);
            pid = 0;
        }

        nk_closePipe(out);
        nk_closePipe(null_pipe);

        nk_setLastError(err);

        ProfEndBlock();
        return false;
    }

    *pipe_stream = (NkPipeStream){
        nk_file_getStream(out.read),
        out.read,
        pid,
    };

    ProfEndBlock();
    return true;
}

bool nk_pipe_streamOpenWrite(NkPipeStream *pipe_stream, nks cmd, bool quiet) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    nksb_try_append_str(&sb, cmd);
    nksb_try_append_null(&sb);

    NK_LOG_DBG("exec(\"" nks_Fmt "\")", nks_Arg(sb));

    nkpipe_t in = nk_createPipe();
    nkpipe_t null_pipe = {nk_invalid_fd, quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd};
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, &in, &null_pipe, &null_pipe) < 0) {
        nkerr_t err = nk_getLastError();

        if (pid > 0) {
            nk_waitpid(pid, NULL);
            pid = 0;
        }

        nk_closePipe(in);
        nk_closePipe(null_pipe);

        nk_setLastError(err);

        ProfEndBlock();
        return false;
    }

    *pipe_stream = (NkPipeStream){
        nk_file_getStream(in.write),
        in.write,
        pid,
    };

    ProfEndBlock();
    return true;
}

i32 nk_pipe_streamClose(NkPipeStream *pipe_stream) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    i32 ret = 1;
    nk_close(pipe_stream->fd);
    if (pipe_stream->pid > 0) {
        nk_waitpid(pipe_stream->pid, &ret);
    }

    *pipe_stream = (NkPipeStream){0};

    ProfEndBlock();
    return ret;
}
