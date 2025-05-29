#include "cc_adapter.h"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "translate_to_c.h"

NK_LOG_USE_SCOPE(cc_adapter);

NkPipeStream nkcc_streamOpen(NkArena *scratch, NkIrCompilerConfig const conf) {
    NK_LOG_TRC("%s", __func__);

    NkPipeStream src;
    NK_PROF_FUNC() {
        NkStringBuilder sb = {0};

        nksb_printf(
            &sb,
            NKS_FMT " -x c - -o " NKS_FMT " -lm " NKS_FMT,
            NKS_ARG(conf.compiler_binary),
            NKS_ARG(conf.output_filename),
            NKS_ARG(conf.additional_flags));

        nk_pipe_streamOpenWrite(scratch, &src, (NkString){NKS_INIT(sb)}, conf.quiet);

        nksb_free(&sb);
    }
    return src;
}

int nkcc_streamClose(NkPipeStream *stream) {
    NK_LOG_TRC("%s", __func__);

    int ret;
    NK_PROF_FUNC() {
        ret = nk_pipe_streamClose(stream);
    }
    return ret;
}

bool nkir_compile(NkArena *scratch, NkIrCompilerConfig const conf, NkIrProg ir, NkIrFunct entry_point) {
    NK_LOG_TRC("%s", __func__);

    int ret;
    NK_PROF_FUNC() {
        NkPipeStream src = nkcc_streamOpen(scratch, conf);
        nkir_translateToC(ir, entry_point, src.stream);
        ret = nkcc_streamClose(&src);
    }
    return ret;
}
