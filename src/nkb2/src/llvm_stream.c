#include "llvm_stream.h"

#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/pipe_stream.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(llvm_stream);

bool nk_llvm_stream_open(NkArena *scratch, NkPipeStream *ps, NkStringBuf opt_buf, NkString out_file, NkStream *out) {
    NK_LOG_TRC("%s", __func__);

    bool ret;

    NK_PROF_FUNC()
    NK_ARENA_SCOPE(scratch) {
        // TODO: Hardcoded -lm
        NkString const cmd = nk_tsprintf(
            scratch,
            "sh -c \"tee -a /dev/stderr | opt -O3 - | llc -O3 --relocation-model=pic - | clang -O3 -x assembler -lm "
            "-o\\\"" NKS_FMT "\\\" -\"",
            NKS_ARG(out_file));
        ret = nk_pipe_streamOpenWrite(
            ps,
            (NkPipeStreamInfo){
                .scratch = scratch,
                .cmd = cmd,
                .opt_buf = opt_buf,
                .quiet = false,
            },
            out);
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
