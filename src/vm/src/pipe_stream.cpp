#include "pipe_stream.hpp"

#include <new>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"

#ifdef __GNUC__
#include <ext/stdio_sync_filebuf.h>
using popen_filebuf = __gnu_cxx::stdio_sync_filebuf<char>;
#elif _MSC_VER
#include <fstream>
using popen_filebuf = std::filebuf;
#else
static_assert(false, "filebuf is not available for this platform");
#endif

namespace {

NK_LOG_USE_SCOPE(pipe_stream);

void _makeCmdStr(NkStringBuilder sb, nkstr cmd, bool quiet) {
    nksb_printf(sb, "%.*s", cmd.size, cmd.data);
    if (quiet) {
        nksb_printf(sb, " >/dev/null 2>&1");
    }
    nksb_printf(sb, "%c", '\0');
}

popen_filebuf *_createFileBuf(FILE *file) {
    return new (nk_alloc(nk_default_allocator, sizeof(popen_filebuf))) popen_filebuf{file};
}

} // namespace

std::istream nk_pipe_streamRead(nkstr cmd, bool quiet) {
    NK_LOG_TRC(__func__);

    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };
    _makeCmdStr(sb, cmd, quiet);
    auto str = nksb_concat(sb);

    auto file = popen(str.data, "r");
    return std::istream{_createFileBuf(file)};
}

std::ostream nk_pipe_streamWrite(nkstr cmd, bool quiet) {
    NK_LOG_TRC(__func__);

    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };
    _makeCmdStr(sb, cmd, quiet);
    auto str = nksb_concat(sb);

    auto file = popen(str.data, "w");
    return std::ostream{_createFileBuf(file)};
}

bool nk_pipe_streamClose(std::ios const &stream) {
    NK_LOG_TRC(__func__);

    auto buf = (popen_filebuf *)stream.rdbuf();
    auto res = pclose(buf->file());
    buf->~popen_filebuf();
    nk_free(nk_default_allocator, buf);
    return !res;
}
