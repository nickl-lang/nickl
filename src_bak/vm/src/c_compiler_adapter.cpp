#include "nk/vm/c_compiler_adapter.hpp"

#include "nk/common/string_builder.hpp"
#include "nk/common/utils.hpp"
#include "pipe_stream.hpp"
#include "translate_to_c.hpp"

namespace nk {
namespace vm {

std::ostream c_compiler_streamOpen(CCompilerConfig const &conf) {
    StringBuilder sb{};
    sb << (conf.quiet ? "" : "tee /dev/stderr |") << " " << conf.compiler_binary << " -x c -o "
       << conf.output_filename << " " << conf.additional_flags << " -";

    LibcAllocator allocator;
    auto cmd = sb.moveStr(allocator);
    defer {
        allocator.free((void *)cmd.data);
    };

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
