#include "cc_adapter.hpp"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
#include "translate_to_c.hpp"

namespace {

NK_LOG_USE_SCOPE(cc_adapter);

} // namespace

nk_stream nkcc_streamOpen(NkIrCompilerConfig const &conf) {
    ProfBeginFunc();
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
    ProfEndBlock();
    return src;
}

int nkcc_streamClose(nk_stream stream) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    auto ret = nk_pipe_streamClose(stream);
    ProfEndBlock();
    return ret;
}

bool nkir_compile(NkIrCompilerConfig conf, NkIrProg ir, NkIrFunct entry_point) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    auto src = nkcc_streamOpen(conf);
    nkir_translateToC(ir, entry_point, src);
    auto ret = nkcc_streamClose(src);
    ProfEndBlock();
    return ret;
}
