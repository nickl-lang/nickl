#ifndef NTK_PROCESS_H_
#define NTK_PROCESS_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkHandle read_file;
    NkHandle write_file;
} NkPipe;

NK_EXPORT NkPipe nk_pipe_create(void);
NK_EXPORT void nk_pipe_close(NkPipe pipe);

NK_EXPORT i32 nk_execAsync(char const *cmd, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err);
NK_EXPORT i32 nk_waitProc(NkHandle process, i32 *exit_status);

NK_INLINE i32 nk_exec(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status) {
    NkHandle process = NK_NULL_HANDLE;
    i32 res = nk_execAsync(cmd, &process, in, out, err);
    nk_waitProc(process, exit_status);
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_PROCESS_H_
