#include "llvm_stream.h"

#include "ntk/pipe_stream.h"
#include "ntk/string.h"

bool nk_llvm_stream_open(NkPipeStream *ps) {
    return nk_pipe_streamOpenWrite(
        ps, nk_cs2s("sh -c \"opt -O3 - | llc --relocation-model=pic - | clang -x assembler -oa.out -\""), false);
}

i32 nk_llvm_stream_close(NkPipeStream *ps) {
    return nk_pipe_streamClose(ps);
}
