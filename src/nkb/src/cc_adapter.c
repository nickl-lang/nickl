#include "cc_adapter.h"

#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(cc_adapter);

bool nkcc_streamOpen(nk_stream *stream, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(sb, 4096);
    nksb_printf(
        &sb,
        nks_Fmt " -x c - -o " nks_Fmt " -lm " nks_Fmt,
        nks_Arg(conf.compiler_binary),
        nks_Arg(conf.output_filename),
        nks_Arg(conf.additional_flags));

    return nk_pipe_streamWrite(stream, (nks){nkav_init(sb)}, conf.quiet);
}

int nkcc_streamClose(nk_stream stream) {
    NK_LOG_TRC("%s", __func__);

    return nk_pipe_streamClose(stream);
}
