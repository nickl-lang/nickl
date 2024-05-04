#include "cc_adapter.h"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "translate_to_c.h"

NK_LOG_USE_SCOPE(cc_adapter);

NkPipeStream nkcc_streamOpen(NkIrCompilerConfig const conf) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb = {0};

    nksb_printf(
        &sb,
        NKS_FMT " -x c - -o " NKS_FMT " -lm " NKS_FMT,
        NKS_ARG(conf.compiler_binary),
        NKS_ARG(conf.output_filename),
        NKS_ARG(conf.additional_flags));

    NkPipeStream src;
    nk_pipe_streamOpenWrite(&src, (NkString){NKS_INIT(sb)}, conf.quiet);

    nksb_free(&sb);
    NK_PROF_FUNC_END();
    return src;
}

int nkcc_streamClose(NkPipeStream *stream) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    int ret = nk_pipe_streamClose(stream);
    NK_PROF_FUNC_END();
    return ret;
}

bool nkir_compile(NkIrCompilerConfig const conf, NkIrProg ir, NkIrFunct entry_point) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    NkPipeStream src = nkcc_streamOpen(conf);
    nkir_translateToC(ir, entry_point, src.stream);
    int ret = nkcc_streamClose(&src);
    NK_PROF_FUNC_END();
    return ret;
}
