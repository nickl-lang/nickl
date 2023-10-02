#include "cc_adapter.h"

#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(cc_adapter);

nk_stream nkcc_streamOpen(NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    NkStringBuilder sb = {0};

    nksb_printf(
        &sb,
        nks_Fmt " -x c - -o " nks_Fmt " -lm " nks_Fmt,
        nks_Arg(conf.compiler_binary),
        nks_Arg(conf.output_filename),
        nks_Arg(conf.additional_flags));

    nk_stream in = nk_pipe_streamWrite((nks){nkav_init(sb)}, conf.quiet);

    nksb_free(&sb);
    return in;
}

int nkcc_streamClose(nk_stream stream) {
    NK_LOG_TRC("%s", __func__);

    return nk_pipe_streamClose(stream);
}
