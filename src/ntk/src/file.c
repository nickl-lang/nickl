#include "ntk/file.h"

#include <string.h>

#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

#define BUF_SIZE 4096

NK_LOG_USE_SCOPE(file);

bool nk_file_read(NkAllocator alloc, NkString filepath, NkString *out) {
    NK_LOG_TRC("%s", __func__);
    NK_LOG_DBG("Reading file `" NKS_FMT "`", NKS_ARG(filepath));

    bool ok = false;
    NK_PROF_FUNC() {
        NKSB_FIXED_BUFFER(path, NK_MAX_PATH);
        nksb_tryAppendStr(&path, filepath);
        nksb_tryAppendNull(&path);

        NkHandle file = nk_open(path.data, NkOpenFlags_Read);
        if (!nk_handleIsNull(file)) {
            NkStringBuilder sb = {.alloc = alloc};
            if (nksb_readFromStreamEx(&sb, nk_file_getStream(file), BUF_SIZE)) {
                *out = (NkString){NKS_INIT(sb)};
                ok = true;
            }
        }
        nk_close(file);
    }
    return ok;
}

static i32 streamProc(void *stream_data, char *buf, usize size, NkStreamMode mode) {
    NkHandle const file = nk_handleFromVoidPtr(stream_data);

    switch (mode) {
        case NkStreamMode_Read:
            return nk_read(file, buf, size);

        case NkStreamMode_Write:
            return nk_write(file, buf, size);

        case NkStreamMode_Flush:
            return nk_flush(file);
    }

    nk_assert(!"unreachable");
    return -1;
}

NkStream nk_file_getStream(NkHandle file) {
    return (NkStream){nk_handleToVoidPtr(file), streamProc};
}

static i32 bufferedWriteStreamFlush(NkFileStreamBuf *stream_buf) {
    i32 ret = nk_write(stream_buf->file, stream_buf->buf, stream_buf->_used);
    stream_buf->_used = 0;
    return ret;
}

static i32 bufferedWriteStreamProc(void *stream_data, char *buf, usize const size, NkStreamMode mode) {
    NkFileStreamBuf *stream_buf = stream_data;

    switch (mode) {
        case NkStreamMode_Write: {
            usize bytes_left_to_write = size;
            while (bytes_left_to_write) {
                usize bytes_available = stream_buf->size - stream_buf->_used;
                if (bytes_available == 0) {
                    bufferedWriteStreamFlush(stream_buf);
                    bytes_available = stream_buf->size;
                }
                usize const bytes_to_write = nk_minu(bytes_available, bytes_left_to_write);
                memcpy(stream_buf->buf + stream_buf->_used, buf, bytes_to_write);
                buf += bytes_to_write;
                bytes_left_to_write -= bytes_to_write;
                stream_buf->_used += bytes_to_write;
            }
            return size;
        }

        case NkStreamMode_Flush:
            return bufferedWriteStreamFlush(stream_buf);

        case NkStreamMode_Read:
            return -1;
    }

    nk_assert(!"unreachable");
    return -1;
}

NkStream nk_file_getBufferedWriteStream(NkFileStreamBuf *stream_buf) {
    return (NkStream){stream_buf, bufferedWriteStreamProc};
}
