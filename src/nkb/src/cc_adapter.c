#include "cc_adapter.h"

#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(cc_adapter);

bool nkcc_streamOpen(NkPipeStream *stream, NkIrCompilerConfig conf) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    NKSB_FIXED_BUFFER(cmd, 4096);

    nksb_printf(
        &cmd,
        NKS_FMT " -x c - -o " NKS_FMT
                " -fPIC -fvisibility=hidden"
                " -Wno-builtin-declaration-mismatch"
                " -Wno-implicit-function-declaration"
                " -Wno-main"
                " -Wno-unused-label"
                " -Wno-unused-parameter",
        NKS_ARG(conf.compiler_binary),
        NKS_ARG(conf.output_filename));

    for (usize i = 0; i < conf.additional_flags.size; i++) {
        nksb_printf(&cmd, " " NKS_FMT, NKS_ARG(conf.additional_flags.data[i]));
    }

    switch (conf.output_kind) {
        case NkbOutput_Object:
            nksb_tryAppendCStr(&cmd, " -c");
            break;
        case NkbOutput_Static:
            nksb_tryAppendCStr(&cmd, " -static");
            break;
        case NkbOutput_Shared:
            nksb_tryAppendCStr(&cmd, " -shared");
            break;
        case NkbOutput_Executable:
            break;

        default:
            nk_assert(!"unreachable");
            break;
    }

    bool ret = nk_pipe_streamOpenWrite(stream, (NkString){NKS_INIT(cmd)}, conf.quiet);
    NK_PROF_FUNC_END();
    return ret;
}

int nkcc_streamClose(NkPipeStream *stream) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    int ret = nk_pipe_streamClose(stream);
    NK_PROF_FUNC_END();
    return ret;
}
