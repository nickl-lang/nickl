#include "nk/common/pipe_stream.h"

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/stream.h"
#include "nk/common/string_builder.h"
#include "nk/sys/error.h"
#include "nk/sys/file.h"
#include "nk/sys/process.h"

NK_LOG_USE_SCOPE(pipe_stream);

static void makeCmdStr(NkStringBuilder *sb, nkstr cmd) {
    nksb_printf(sb, nkstr_Fmt, nkstr_Arg(cmd));
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

nk_stream nk_pipe_streamRead(nkstr cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    makeCmdStr(&sb, cmd);

    NK_LOG_DBG("exec(\"" nkstr_Fmt "\")", nkstr_Arg(sb));

    nkpipe_t out = nk_createPipe();
    nkpipe_t null_pipe = {
        quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd,
        quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd};
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, NULL, &out, &null_pipe) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"" nkstr_Fmt "\") failed: %s", nkstr_Arg(sb), nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, NULL);
        }
        nk_close(out.read);
        nk_close(out.write);

        return (nk_stream){0};
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){out.read, pid};

    return (nk_stream){context, nk_pipe_streamReadProc};
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

nk_stream nk_pipe_streamWrite(nkstr cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, CMD_BUF_SIZE);
    makeCmdStr(&sb, cmd);

    NK_LOG_DBG("exec(\"" nkstr_Fmt "\")", nkstr_Arg(sb));

    nkpipe_t in = nk_createPipe();
    nkpipe_t null_pipe = {nk_invalid_fd, quiet ? nk_open(nk_null_file, nk_open_write) : nk_invalid_fd};
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, &in, &null_pipe, &null_pipe) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"" nkstr_Fmt "\") failed: %s", nkstr_Arg(sb), nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, NULL);
        }
        nk_close(in.read);
        nk_close(in.write);

        return (nk_stream){0};
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){in.write, pid};

    return (nk_stream){context, nk_pipe_streamWriteProc};
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
