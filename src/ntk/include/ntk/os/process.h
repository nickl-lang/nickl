#ifndef NTK_OS_PROCESS_H_
#define NTK_OS_PROCESS_H_

#include "ntk/common.h"
#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkOsHandle h_read;
    NkOsHandle h_write;
} NkPipe;

NkPipe nk_proc_createPipe(void);
void nk_proc_closePipe(NkPipe pipe);

i32 nk_proc_execAsync(char const *cmd, NkOsHandle *h_process, NkPipe *in, NkPipe *out, NkPipe *err);
i32 nk_proc_wait(NkOsHandle h_process, i32 *exit_status);

NK_INLINE i32 nk_proc_execSync(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status) {
    NkOsHandle h_process = NK_OS_HANDLE_ZERO;
    i32 res = nk_proc_execAsync(cmd, &h_process, in, out, err);
    nk_proc_wait(h_process, exit_status);
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_PROCESS_H_
