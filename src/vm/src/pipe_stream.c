#include "pipe_stream.h"

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/stream.h"
#include "nk/common/string_builder.h"
#include "nk/sys/error.h"
#include "nk/sys/file.h"
#include "nk/sys/process.h"

NK_LOG_USE_SCOPE(pipe_stream);

static void makeCmdStr(NkStringBuilder *sb, nkstr cmd, bool quiet) {
    nksb_printf(sb, nkstr_Fmt, nkstr_Arg(cmd));
    if (quiet) {
        nksb_printf(sb, " >/dev/null 2>&1");
    }
    nksb_printf(sb, "%c", '\0');
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

nk_stream nk_pipe_streamRead(nkstr cmd, bool quiet) {
    NK_LOG_TRC("%s", __func__);

    nk_stream in = {0};

    NkStringBuilder sb = {0};
    makeCmdStr(&sb, cmd, quiet);

    NK_LOG_DBG("exec(\"" nkstr_Fmt "\")", nkstr_Arg(sb));

    nkpipe_t out = nk_createPipe();
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, NULL, &out) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"" nkstr_Fmt "\") failed: %s", nkstr_Arg(sb), nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, NULL);
        }
        nk_close(out.read);
        nk_close(out.write);

        goto defer;
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){out.read, pid};

    in.data = context;
    in.proc = nk_pipe_streamReadProc;

defer:
    nksb_free(&sb);
    return in;
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

    nk_stream out = {0};

    NkStringBuilder sb = {0};
    makeCmdStr(&sb, cmd, quiet);

    NK_LOG_DBG("exec(\"" nkstr_Fmt "\")", nkstr_Arg(sb));

    nkpipe_t in = nk_createPipe();
    nkpid_t pid = 0;
    if (nk_execAsync(sb.data, &pid, &in, NULL) < 0) {
        // TODO Report errors to the user
        NK_LOG_ERR("exec(\"" nkstr_Fmt "\") failed: %s", nkstr_Arg(sb), nk_getLastErrorString());
        if (pid > 0) {
            nk_waitpid(pid, NULL);
        }
        nk_close(in.read);
        nk_close(in.write);

        goto defer;
    }

    PipeStreamContext *context = (PipeStreamContext *)nk_alloc(nk_default_allocator, sizeof(PipeStreamContext));
    *context = (PipeStreamContext){in.write, pid};

    out.data = context;
    out.proc = nk_pipe_streamWriteProc;

defer:
    nksb_free(&sb);
    return out;
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
