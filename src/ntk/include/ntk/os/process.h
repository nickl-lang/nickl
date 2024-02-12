#ifndef NTK_OS_PROCESS_H_
#define NTK_OS_PROCESS_H_

#include "ntk/common.h"
#include "ntk/os/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkpid_t;

typedef struct {
    nkfd_t read;
    nkfd_t write;
} NkPipe;

NkPipe nk_proc_createPipe(void);
void nk_proc_closePipe(NkPipe pipe);

i32 nk_proc_execAsync(char const *cmd, nkpid_t *pid, NkPipe *in, NkPipe *out, NkPipe *err);
i32 nk_proc_waitpid(nkpid_t pid, i32 *exit_status);

NK_INLINE i32 nk_proc_execSync(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status) {
    nkpid_t pid = 0;
    i32 res = nk_proc_execAsync(cmd, &pid, in, out, err);
    if (pid > 0) {
        nk_proc_waitpid(pid, exit_status);
    }
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_PROCESS_H_
