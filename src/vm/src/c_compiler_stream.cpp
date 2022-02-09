#include "c_compiler_stream.hpp"

#include "nk/common/utils.hpp"
#include "pipe_stream.hpp"

namespace nk {
namespace vm {

std::ostream ccompiler_streamOpen(CCompilerConfig const &conf) {
    auto cmd = string_format(
        *_mctx.tmp_allocator,
        "%.*s -x c -o %.*s %.*s -",
        conf.compiler_binary.size,
        conf.compiler_binary.data,
        conf.output_filename.size,
        conf.output_filename.data,
        conf.additional_flags.size,
        conf.additional_flags.data);
    return pipe_streamWrite(cmd, conf.quiet);
}

bool ccompiler_streamClose(std::ostream const &stream) {
    return pipe_streamClose(stream);
}

} // namespace vm
} // namespace nk
