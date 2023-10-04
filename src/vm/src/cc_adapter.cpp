#include "cc_adapter.hpp"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.hpp"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
#include "translate_to_c.hpp"

namespace {

NK_LOG_USE_SCOPE(cc_adapter);

} // namespace

nk_stream nkcc_streamOpen(NkIrCompilerConfig const &conf) {
    EASY_FUNCTION(::profiler::colors::Amber200);
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(
        &sb,
        nks_Fmt " -x c - -o " nks_Fmt " -lm " nks_Fmt,
        nks_Arg(conf.compiler_binary),
        nks_Arg(conf.output_filename),
        nks_Arg(conf.additional_flags));

    nk_stream src{};
    nk_pipe_streamWrite(&src, {nkav_init(sb)}, conf.quiet);
    return src;
}

int nkcc_streamClose(nk_stream stream) {
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
