#ifndef HEADER_GUARD_NTK_SYS_PROCESS
#define HEADER_GUARD_NTK_SYS_PROCESS

#include "ntk/common.h"
#include "ntk/sys/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkpid_t;

typedef struct {
    nkfd_t read;
    nkfd_t write;
} nkpipe_t;

nkpipe_t nk_createPipe(void);
void nk_closePipe(nkpipe_t pipe);

i32 nk_execAsync(char const *cmd, nkpid_t *pid, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err);
i32 nk_waitpid(nkpid_t pid, i32 *exit_status);

NK_INLINE i32 nk_execSync(char const *cmd, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err, i32 *exit_status) {
    nkpid_t pid = 0;
    i32 res = nk_execAsync(cmd, &pid, in, out, err);
    if (pid > 0) {
        nk_waitpid(pid, exit_status);
    }
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_PROCESS
