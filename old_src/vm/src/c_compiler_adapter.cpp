#include "c_compiler_adapter.hpp"

#include "nk/str/static_string_builder.hpp"
#include "nk/utils/utils.hpp"
#include "pipe_stream.hpp"
#include "translate_to_c.hpp"

namespace nk {
namespace vm {

namespace {

static constexpr size_t c_buf_size = 1024;

} // namespace

std::ostream c_compiler_streamOpen(CCompilerConfig const &conf) {
    ARRAY_SLICE(char, cmd, c_buf_size);

    StaticStringBuilder{cmd} << (conf.quiet ? "" : "tee /dev/stderr |") << " "
                             << conf.compiler_binary << " -x c -o " << conf.output_filename << " "
                             << conf.additional_flags << " -";

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
