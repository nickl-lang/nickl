#include "cc_adapter.hpp"

#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/ir.h"
#include "pipe_stream.hpp"
#include "translate_to_c.hpp"

namespace {

NK_LOG_USE_SCOPE(cc_adapter);

} // namespace

std::ostream nkcc_streamOpen(CCompilerConfig const &conf) {
    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(
        sb,
        "%s %s -x c -o %s %s -",
        conf.quiet ? "" : "tee /dev/stderr |",
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

bool nkcc_compile(CCompilerConfig const &conf, NkIrProg ir) {
    auto src = nkcc_streamOpen(conf);
    // TODO nkir_translateToC(ir, src);
    return nkcc_streamClose(src);
}