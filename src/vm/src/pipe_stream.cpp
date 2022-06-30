#include "pipe_stream.hpp"

#include "nk/common/logger.h"
#include "nk/common/mem.h"
#include "nk/common/static_string_builder.hpp"
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

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::pipe_stream);

void _makeCmdStr(StaticStringBuilder &sb, string cmd, bool quiet) {
    sb << cmd;
    if (quiet) {
        sb << " >/dev/null 2>&1";
    }
}

popen_filebuf *_createFileBuf(FILE *file) {
    return new (nk_platform_alloc_aligned(sizeof(popen_filebuf), alignof(popen_filebuf)))
        popen_filebuf{file};
}

static constexpr size_t c_buf_size = 1024;

} // namespace

std::istream pipe_streamRead(string cmd, bool quiet) {
    LOG_TRC(__func__);

    char buf[c_buf_size];
    Slice<char> str{buf, c_buf_size};
    StaticStringBuilder sb{str};
    _makeCmdStr(sb, cmd, quiet);

    auto file = popen(str.data, "r");
    return std::istream{_createFileBuf(file)};
}

std::ostream pipe_streamWrite(string cmd, bool quiet) {
    LOG_TRC(__func__);

    char buf[c_buf_size];
    Slice<char> str{buf, c_buf_size};
    StaticStringBuilder sb{str};
    _makeCmdStr(sb, cmd, quiet);

    auto file = popen(str.data, "w");
    return std::ostream{_createFileBuf(file)};
}

bool pipe_streamClose(std::ios const &stream) {
    LOG_TRC(__func__);

    auto buf = (popen_filebuf *)stream.rdbuf();
    auto res = pclose(buf->file());
    buf->~popen_filebuf();
    nk_platform_free_aligned(buf);
    return !res;
}

} // namespace vm
} // namespace nk
