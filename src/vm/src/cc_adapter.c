#include "cc_adapter.h"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
#include "translate_to_c.h"

NK_LOG_USE_SCOPE(cc_adapter);

NkPipeStream nkcc_streamOpen(NkIrCompilerConfig const conf) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb = {0};

    nksb_printf(
        &sb,
        nks_Fmt " -x c - -o " nks_Fmt " -lm " nks_Fmt,
        nks_Arg(conf.compiler_binary),
        nks_Arg(conf.output_filename),
        nks_Arg(conf.additional_flags));

    NkPipeStream src;
    nk_pipe_streamOpenWrite(&src, (nks){nkav_init(sb)}, conf.quiet);

    nksb_free(&sb);
    ProfEndBlock();
    return src;
}

int nkcc_streamClose(NkPipeStream *stream) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    int ret = nk_pipe_streamClose(stream);
    ProfEndBlock();
    return ret;
}

bool nkir_compile(NkIrCompilerConfig const conf, NkIrProg ir, NkIrFunct entry_point) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    NkPipeStream src = nkcc_streamOpen(conf);
    nkir_translateToC(ir, entry_point, src.stream);
    int ret = nkcc_streamClose(&src);
    ProfEndBlock();
    return ret;
}
