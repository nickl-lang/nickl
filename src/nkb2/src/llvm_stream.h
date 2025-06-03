#ifndef NKB_LLVM_STREAM_H_
#define NKB_LLVM_STREAM_H_

#include "ntk/arena.h"
#include "ntk/pipe_stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkArena *scratch;
    NkPipeStream *ps;
    NkStringBuf opt_buf;
    NkString out_file;
} NkLlvmStreamInfo;

bool nk_llvm_stream_open(NkLlvmStreamInfo const info, NkStream *out);
i32 nk_llvm_stream_close(NkPipeStream *ps);

#ifdef __cplusplus
}
#endif

#endif // NKB_LLVM_STREAM_H_
