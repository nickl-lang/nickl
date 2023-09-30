#include "nk/common/file.h"

#include "nk/common/string_builder.h"

NkFileReadResult nk_file_read(NkAllocator alloc, nkstr file) {
    nk_stream stream = nk_file_openStream(file, nk_open_read);
    if (!stream.proc) {
        return (NkFileReadResult){0};
    }

    NkStringBuilder sb = {nksb_init(alloc)};
    nksb_readFromStream(&sb, stream);
    nk_file_closeStream(stream);

    return (NkFileReadResult){
        .bytes = {nkav_init(sb)},
        .ok = true,
    };
}

static int nk_file_streamProc(void *stream_data, char *buf, size_t size, nk_stream_mode mode) {
    nkfd_t fd = (nkfd_t)stream_data;

    switch (mode) {
    case nk_stream_mode_read:
        return nk_read(fd, buf, size);
    case nk_stream_mode_write:
        return nk_write(fd, buf, size);
    default:
        return -1;
    }
}

#define MAX_PATH 4096

nk_stream nk_file_openStream(nkstr file, int flags) {
    nksb_fixed_buffer(sb, MAX_PATH);
    nksb_try_append_many(&sb, file.data, file.size);
    nksb_try_append_null(&sb);

    nkfd_t fd = nk_open(sb.data, flags);
    if (fd == nk_invalid_fd) {
        return (nk_stream){0};
    } else {
        return (nk_stream){(void *)fd, nk_file_streamProc};
    }
}

void nk_file_closeStream(nk_stream stream) {
    nk_close((nkfd_t)stream.data);
}
