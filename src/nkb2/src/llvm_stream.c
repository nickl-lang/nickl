#include "llvm_stream.h"

#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(llvm_stream);

bool nk_llvm_stream_open(NkLlvmStreamInfo const info, NkStream *out) {
    NK_LOG_TRC("%s", __func__);

    bool ret;

    NK_PROF_FUNC() {
        NK_ARENA_SCOPE(info.scratch) {
            // TODO: Hardcoded -lm
            NkString const cmd = nk_tsprintf(
                info.scratch,
                "sh -c \"opt -O3 - | llc -O3 --relocation-model=pic --filetype=obj -o \\\"" NKS_FMT "\\\"\"",
                NKS_ARG(info.out_file));
            ret = nk_pipe_streamOpenWrite(
                (NkPipeStreamInfo){
                    .ps = info.ps,
                    .scratch = info.scratch,
                    .cmd = cmd,
                    .opt_buf = info.opt_buf,
                    .quiet = false,
                },
                out);
        }
    }

    return ret;
}

i32 nk_llvm_stream_close(NkPipeStream *ps) {
    NK_LOG_TRC("%s", __func__);

    i32 ret;
    NK_PROF_FUNC() {
        ret = nk_pipe_streamClose(ps);
    }
    return ret;
}
