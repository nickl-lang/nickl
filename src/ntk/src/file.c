#include "ntk/file.h"

#include "ntk/os/common.h"
#include "ntk/os/file.h"
#include "ntk/os/path.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

#define BUF_SIZE 4096

NkFileReadResult nk_file_read(NkAllocator alloc, NkString file) {
    NK_PROF_FUNC_BEGIN();

    NKSB_FIXED_BUFFER(path, NK_MAX_PATH);
    nksb_tryAppendStr(&path, file);
    nksb_tryAppendNull(&path);

    NkFileReadResult res = {0};

    NkOsHandle h_file = nk_open(path.data, NkOpenFlags_Read);
    if (!nkos_handleIsZero(h_file)) {
        NkStringBuilder sb = {NKSB_INIT(alloc)};
        if (nksb_readFromStreamEx(&sb, nk_file_getStream(h_file), BUF_SIZE)) {
            res.bytes = (NkString){NKS_INIT(sb)};
            res.ok = true;
        }
    }
    nk_close(h_file);

    NK_PROF_FUNC_END();
    return res;
}

static i32 nk_file_streamProc(void *stream_data, char *buf, usize size, NkStreamMode mode) {
    NkOsHandle h_file = nkos_handleFromVoidPtr(stream_data);

    switch (mode) {
        case NkStreamMode_Read:
            return nk_read(h_file, buf, size);
        case NkStreamMode_Write:
            return nk_write(h_file, buf, size);
        default:
            return -1;
    }
}

NkStream nk_file_getStream(NkOsHandle h_file) {
    return (NkStream){nkos_handleToVoidPtr(h_file), nk_file_streamProc};
}
