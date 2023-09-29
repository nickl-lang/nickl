#include "cc_adapter.hpp"

#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
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
    EASY_FUNCTION(::profiler::colors::Amber200);
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    auto echo_cmd = nk_mkstr(conf.echo_src ? "tee /dev/stderr | " : "");
    nksb_printf(
        &sb,
        nkstr_Fmt nkstr_Fmt " -x c - -o " nkstr_Fmt " -lm " nkstr_Fmt,
        nkstr_Arg(echo_cmd),
        nkstr_Arg(conf.compiler_binary),
        nkstr_Arg(conf.output_filename),
        nkstr_Arg(conf.additional_flags));

    return nk_pipe_streamWrite({nkav_init(sb)}, conf.quiet);
}

bool nkcc_streamClose(std::ostream const &stream) {
    EASY_FUNCTION(::profiler::colors::Amber200);
    NK_LOG_TRC("%s", __func__);

    return nk_pipe_streamClose(stream);
}

bool nkir_compile(NkIrCompilerConfig conf, NkIrProg ir, NkIrFunct entry_point) {
    EASY_FUNCTION(::profiler::colors::Amber200);
    NK_LOG_TRC("%s", __func__);

    auto src = nkcc_streamOpen(conf);
    nkir_translateToC(ir, entry_point, src);
    return nkcc_streamClose(src);
}
