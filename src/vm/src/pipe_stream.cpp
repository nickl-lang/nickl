#include "pipe_stream.hpp"

#include "nk/common/logger.h"
#include "nk/common/string_builder.hpp"
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

void _makeCmdStr(StringBuilder &sb, string cmd, bool quiet) {
    sb << cmd;
    if (quiet) {
        sb << " >/dev/null 2>&1";
    }
}

popen_filebuf *_createFileBuf(FILE *file) {
    return new (_mctx.def_allocator->alloc<popen_filebuf>()) popen_filebuf{file};
}

} // namespace

std::istream pipe_streamRead(string cmd, bool quiet) {
    LOG_TRC(__FUNCTION__);

    StringBuilder sb{};
    _makeCmdStr(sb, cmd, quiet);
    LibcAllocator allocator;
    auto str = sb.moveStr(allocator);
    defer {
        allocator.free((void *)str.data);
    };

    auto file = popen(str.data, "r");
    return std::istream{_createFileBuf(file)};
}

std::ostream pipe_streamWrite(string cmd, bool quiet) {
    LOG_TRC(__FUNCTION__);

    StringBuilder sb{};
    _makeCmdStr(sb, cmd, quiet);
    LibcAllocator allocator;
    auto str = sb.moveStr(allocator);
    defer {
        allocator.free((void *)str.data);
    };

    auto file = popen(str.data, "w");
    return std::ostream{_createFileBuf(file)};
}

bool pipe_streamClose(std::ios const &stream) {
    LOG_TRC(__FUNCTION__);

    auto buf = (popen_filebuf *)stream.rdbuf();
    auto res = pclose(buf->file());
    buf->~popen_filebuf();
    _mctx.def_allocator->free_aligned(buf);
    return !res;
}

} // namespace vm
} // namespace nk
