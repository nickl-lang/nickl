#include "cc_adapter.h"

#include "ntk/logger.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(cc_adapter);

bool nkcc_streamOpen(NkPipeStream *stream, NkIrCompilerConfig conf) {
    ProfBeginFunc();
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

    for (usize i = 0; i < conf.additional_flags.size; i++) {
        nksb_printf(&cmd, " " nks_Fmt, nks_Arg(conf.additional_flags.data[i]));
    }

    switch (conf.output_kind) {
    case NkbOutput_Object:
        nksb_try_append_cstr(&cmd, " -c");
        break;
    case NkbOutput_Static:
        nksb_try_append_cstr(&cmd, " -static");
        break;
    case NkbOutput_Shared:
        nksb_try_append_cstr(&cmd, " -shared");
        break;
    case NkbOutput_Executable:
        break;

    default:
        assert(!"unreachable");
        break;
    }

    bool ret = nk_pipe_streamOpenWrite(stream, (nks){nkav_init(cmd)}, conf.quiet);
    ProfEndBlock();
    return ret;
}

int nkcc_streamClose(NkPipeStream *stream) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    int ret = nk_pipe_streamClose(stream);
    ProfEndBlock();
    return ret;
}
