#include "nk/vm/c_compiler_adapter.hpp"

#include "nk/common/utils.hpp"
#include "pipe_stream.hpp"
#include "translate_to_c.hpp"

namespace nk {
namespace vm {

std::ostream c_compiler_streamOpen(CCompilerConfig const &conf) {
    auto cmd = tmpstr_format(
        "%s %.*s -x c -o %.*s %.*s -",
        conf.quiet ? "" : "tee /dev/stderr |",
        conf.compiler_binary.size,
        conf.compiler_binary.data,
        conf.output_filename.size,
        conf.output_filename.data,
        conf.additional_flags.size,
        conf.additional_flags.data);
    return pipe_streamWrite(cmd, conf.quiet);
}

bool c_compiler_streamClose(std::ostream const &stream) {
    return pipe_streamClose(stream);
}

bool c_compiler_compile(CCompilerConfig const &conf, ir::Program const &ir) {
    auto src = c_compiler_streamOpen(conf);
    translateToC(ir, src);
    return c_compiler_streamClose(src);
}

} // namespace vm
} // namespace nk
