#include "ntk/pipe_stream.h"

#include "ntk/allocator.h"
#include "ntk/logger.h"
#include "ntk/stream.h"
#include "ntk/string_builder.h"
#include "ntk/sys/error.h"
#include "ntk/sys/file.h"
#include "ntk/sys/process.h"

NK_LOG_USE_SCOPE(pipe_stream);

static void makeCmdStr(NkStringBuilder *sb, nks cmd) {
    nksb_printf(sb, nks_Fmt, nks_Arg(cmd));
    nksb_append_null(sb);
}

typedef struct {
    nkfd_t fd;
    nkpid_t pid;
} PipeStreamContext;

static int nk_pipe_streamReadProc(void *stream_data, char *buf, size_t size, nk_stream_mode mode) {
    if (mode == nk_stream_mode_read) {
        PipeStreamContext *context = stream_data;
        int res = nk_read(context->fd, buf, size);
        if (res < 0) {
            NK_LOG_ERR("failed to read from stream: %s", nk_getLastErrorString());
            res = 0;
        }
        return res;
    } else {
        return -1;
    }
}

#define CMD_BUF_SIZE 4096

bool nk_pipe_streamRead(nk_stream *stream, nks cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    makeCmdStr(&sb, cmd);

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

        *stream = (nk_stream){0};

        return false;
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){out.read, pid};
    *stream = (nk_stream){context, nk_pipe_streamReadProc};

    return true;
}

static int nk_pipe_streamWriteProc(void *stream_data, char *buf, size_t size, nk_stream_mode mode) {
    if (mode == nk_stream_mode_write) {
        PipeStreamContext *context = stream_data;
        int res = nk_write(context->fd, buf, size);
        if (res < 0) {
            NK_LOG_ERR("failed to write to stream: %s", nk_getLastErrorString());
            res = 0;
        }
        return res;
    } else {
        return -1;
    }
}

bool nk_pipe_streamWrite(nk_stream *stream, nks cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    makeCmdStr(&sb, cmd);

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

        *stream = (nk_stream){0};

        return false;
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){in.write, pid};
    *stream = (nk_stream){context, nk_pipe_streamWriteProc};

    return true;
}

int nk_pipe_streamClose(nk_stream stream) {
    NK_LOG_TRC("%s", __func__);

    int ret = 1;
    PipeStreamContext *context = stream.data;
    if (context) {
        nk_close(context->fd);
        if (context->pid > 0) {
            nk_waitpid(context->pid, &ret);
        }
        nk_free(nk_default_allocator, context, sizeof(*context));
    }
    return ret;
}
