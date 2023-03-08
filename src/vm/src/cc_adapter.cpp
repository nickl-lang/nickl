#include "cc_adapter.hpp"

#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "pipe_stream.hpp"
#include "translate_to_c.hpp"

namespace {

NK_LOG_USE_SCOPE(cc_adapter);

} // namespace

std::ostream nkcc_streamOpen(NkIrCompilerConfig const &conf) {
    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(
        sb,
        "%s %s -x c - -o %s -lm %s",
        conf.echo_src ? "tee /dev/stderr |" : "",
        conf.compiler_binary.data,
        conf.output_filename.data,
        conf.additional_flags.data);

    auto cmd = nksb_concat(sb);

    NK_LOG_DBG("cmd: %.*s", cmd.size, cmd.data);

    return nk_pipe_streamWrite(cmd, conf.quiet);
}

bool nkcc_streamClose(std::ostream const &stream) {
    return nk_pipe_streamClose(stream);
}

bool nkir_compile(NkIrCompilerConfig conf, NkIrProg ir, NkIrFunct entry_point) {
    auto src = nkcc_streamOpen(conf);
    nkir_translateToC(ir, entry_point, src);
    return nkcc_streamClose(src);
}
