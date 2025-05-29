#ifndef NKB_LLVM_STREAM_H_
#define NKB_LLVM_STREAM_H_

#include "ntk/arena.h"
#include "ntk/pipe_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nk_llvm_stream_open(NkArena *scratch, NkPipeStream *ps, NkString out_file);
i32 nk_llvm_stream_close(NkPipeStream *ps);

#ifdef __cplusplus
}
#endif

#endif // NKB_LLVM_STREAM_H_
