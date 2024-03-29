#include "cc_adapter.h"

#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(cc_adapter);

bool nkcc_streamOpen(nk_stream *stream, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    nksb_fixed_buffer(cmd, 4096);

    nksb_printf(
        &cmd,
        nks_Fmt " -x c - -o " nks_Fmt
                " -fPIC -fvisibility=hidden"
                " -Wno-builtin-declaration-mismatch"
                " -Wno-implicit-function-declaration"
                " -Wno-main"
                " -Wno-unused-label"
                " -Wno-unused-parameter",
        nks_Arg(conf.compiler_binary),
        nks_Arg(conf.output_filename));

    for (size_t i = 0; i < conf.additional_flags.size; i++) {
        nksb_printf(&cmd, " " nks_Fmt, nks_Arg(conf.additional_flags.data[i]));
    }

    switch (conf.output_kind) {
    case NkbOutput_Object:
        nksb_try_append_str(&cmd, " -c");
        break;
    case NkbOutput_Static:
        nksb_try_append_str(&cmd, " -static");
        break;
    case NkbOutput_Shared:
        nksb_try_append_str(&cmd, " -shared");
        break;
    case NkbOutput_Executable:
        break;

    default:
        assert(!"unreachable");
        break;
    }

    return nk_pipe_streamWrite(stream, (nks){nkav_init(cmd)}, conf.quiet);
}

int nkcc_streamClose(nk_stream stream) {
    NK_LOG_TRC("%s", __func__);

    return nk_pipe_streamClose(stream);
}
