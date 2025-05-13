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

NK_EXPORT NkPipe nk_proc_createPipe(void);
NK_EXPORT void nk_proc_closePipe(NkPipe pipe);

NK_EXPORT i32 nk_proc_execAsync(char const *cmd, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err);
NK_EXPORT i32 nk_proc_wait(NkHandle process, i32 *exit_status);

NK_INLINE i32 nk_proc_execSync(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status) {
    NkHandle process = NK_HANDLE_ZERO;
    i32 res = nk_proc_execAsync(cmd, &process, in, out, err);
    nk_proc_wait(process, exit_status);
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_PROCESS_H_
