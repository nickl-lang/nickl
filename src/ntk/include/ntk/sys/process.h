#ifndef HEADER_GUARD_NTK_SYS_PROCESS
#define HEADER_GUARD_NTK_SYS_PROCESS

#include <stddef.h>
#include <stdint.h>

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

int nk_execAsync(char const *cmd, nkpid_t *pid, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err);
int nk_waitpid(nkpid_t pid, int *exit_status);

NK_INLINE int nk_execSync(char const *cmd, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err, int *exit_status) {
    nkpid_t pid = 0;
    int res = nk_execAsync(cmd, &pid, in, out, err);
    if (pid > 0) {
        nk_waitpid(pid, exit_status);
    }
    return res;
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_PROCESS
