#include "cc_adapter.h"

#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "translate_to_c.h"

NK_LOG_USE_SCOPE(cc_adapter);

void nkcc_streamOpen(
    NkArena *scratch,
    NkPipeStream *ps,
    NkStringBuf opt_buf,
    NkIrCompilerConfig const conf,
    NkStream *out) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        NkStringBuilder sb = {0};

        nksb_printf(
            &sb,
            NKS_FMT " -x c - -o " NKS_FMT " -lm " NKS_FMT,
            NKS_ARG(conf.compiler_binary),
            NKS_ARG(conf.output_filename),
            NKS_ARG(conf.additional_flags));

        nk_pipe_streamOpenWrite(
            ps,
            (NkPipeStreamInfo){
                .scratch = scratch,
                .cmd = {NKS_INIT(sb)},
                .opt_buf = opt_buf,
                .quiet = conf.quiet,
            },
            out);

        nksb_free(&sb);
    }
}

int nkcc_streamClose(NkPipeStream *ps) {
    NK_LOG_TRC("%s", __func__);

    int ret;
    NK_PROF_FUNC() {
        ret = nk_pipe_streamClose(ps);
    }
    return ret;
}

bool nkir_compile(NkArena *scratch, NkIrCompilerConfig const conf, NkIrProg ir, NkIrFunct entry_point) {
    NK_LOG_TRC("%s", __func__);

    int ret;
    NK_PROF_FUNC() {
        char buf[512];
        NkStream src;
        NkPipeStream ps;
        nkcc_streamOpen(scratch, &ps, NK_STATIC_BUF(buf), conf, &src);
        nkir_translateToC(ir, entry_point, src);
        ret = nkcc_streamClose(&ps);
    }
    return ret;
}
