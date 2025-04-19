#include "ntk/file.h"

#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

#define BUF_SIZE 4096

NK_LOG_USE_SCOPE(file);

bool nk_file_read(NkAllocator alloc, NkString filepath, NkString *out) {
    NK_LOG_TRC("%s", __func__);
    NK_LOG_DBG("Reading file `" NKS_FMT "`", NKS_ARG(filepath));

    NK_PROF_FUNC_BEGIN();

    NKSB_FIXED_BUFFER(path, NK_MAX_PATH);
    nksb_tryAppendStr(&path, filepath);
    nksb_tryAppendNull(&path);

    bool ok = false;

    NkHandle h_file = nk_open(path.data, NkOpenFlags_Read);
    if (!nk_handleIsZero(h_file)) {
        NkStringBuilder sb = {NKSB_INIT(alloc)};
        if (nksb_readFromStreamEx(&sb, nk_file_getStream(h_file), BUF_SIZE)) {
            *out = (NkString){NKS_INIT(sb)};
            ok = true;
        }
    }
    nk_close(h_file);

    NK_PROF_FUNC_END();
    return ok;
}

static i32 nk_file_streamProc(void *stream_data, char *buf, usize size, NkStreamMode mode) {
    NkHandle h_file = nk_handleFromVoidPtr(stream_data);

    switch (mode) {
        case NkStreamMode_Read:
            return nk_read(h_file, buf, size);
        case NkStreamMode_Write:
            return nk_write(h_file, buf, size);
        default:
            return -1;
    }
}

NkStream nk_file_getStream(NkHandle h_file) {
    return (NkStream){nk_handleToVoidPtr(h_file), nk_file_streamProc};
}
