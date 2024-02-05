#include "ntk/file.h"

#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/sys/file.h"
#include "ntk/sys/path.h"

NkFileReadResult nk_file_read(NkAllocator alloc, nks file) {
    ProfBeginFunc();

    nksb_fixed_buffer(path, NK_MAX_PATH);
    nksb_try_append_many(&path, file.data, file.size);
    nksb_try_append_null(&path);

    nkfd_t fd = nk_open(path.data, nk_open_read);

    if (fd < 0) {
        ProfEndBlock();
        return (NkFileReadResult){0};
    }

    NkStringBuilder sb = {nksb_init(alloc)};
    nksb_readFromStream(&sb, nk_file_getStream(fd));
    nk_close(fd);

    ProfEndBlock();
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

nk_stream nk_file_getStream(nkfd_t fd) {
    return (nk_stream){(void *)fd, nk_file_streamProc};
}
