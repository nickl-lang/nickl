#include "c_compiler_adapter.hpp"

#include "nk/common/static_string_builder.hpp"
#include "nk/common/utils.hpp"
#include "pipe_stream.hpp"

namespace nk {
namespace vm {

namespace {

static constexpr size_t c_buf_size = 1024;

} // namespace

std::ostream c_compiler_streamOpen(CCompilerConfig const &conf) {
    char buf[c_buf_size];
    Slice<char> cmd{buf, c_buf_size};
    StaticStringBuilder sb{cmd};

    sb << (conf.quiet ? "" : "tee /dev/stderr |") << " " << conf.compiler_binary << " -x c -o "
       << conf.output_filename << " " << conf.additional_flags << " -";

    return pipe_streamWrite(cmd, conf.quiet);
}

bool c_compiler_streamClose(std::ostream const &stream) {
    return pipe_streamClose(stream);
}

} // namespace vm
} // namespace nk