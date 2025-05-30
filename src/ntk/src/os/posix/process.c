#include "ntk/process.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "common.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/string.h"

NkPipe nk_pipe_create(void) {
    NkPipe pip = {0};

    i32 pipefd[2];
    if (pipe(pipefd) == 0) {
        pip.read_file = fd2handle(pipefd[0]);
        pip.write_file = fd2handle(pipefd[1]);
    }

    return pip;
}

void nk_pipe_close(NkPipe pipe) {
    nk_close(pipe.read_file);
    nk_close(pipe.write_file);
}

static i32 execAsyncImpl(char const *const *args, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err) {
    i32 err_pipe[2];
    if (pipe(err_pipe) < 0) {
        return -1;
    }

    i32 error_code = 0;

    pid_t pid = fork();
    switch (pid) {
        case -1:
            return -1;

        case 0:
            close(err_pipe[0]);
            fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);

            if (in) {
                if (!nk_handleIsNull(in->read_file) && dup2(handle2fd(in->read_file), STDIN_FILENO) < 0) {
                    goto error;
                }
                nk_close(in->write_file);
            }

            if (out) {
                if (!nk_handleIsNull(out->write_file) && dup2(handle2fd(out->write_file), STDOUT_FILENO) < 0) {
                    goto error;
                }
                nk_close(out->read_file);
            }

            if (err) {
                if (!nk_handleIsNull(err->write_file) && dup2(handle2fd(err->write_file), STDERR_FILENO) < 0) {
                    goto error;
                }
                nk_close(err->read_file);
            }

            execvp(args[0], (char *const *)args);

        error:
            error_code = errno;
            (void)!write(err_pipe[1], &error_code, sizeof(error_code));
            _exit(EX_OSERR);

        default:
            *process = pid2handle(pid);

            if (in) {
                nk_close(in->read_file);
            }

            if (out) {
                nk_close(out->write_file);
            }

            if (err) {
                nk_close(err->write_file);
            }

            close(err_pipe[1]);
            (void)!read(err_pipe[0], &error_code, sizeof(error_code));
            close(err_pipe[0]);
            errno = error_code;

            return errno ? -1 : 0;
    }
}

i32 nk_execAsync(NkArena *scratch, NkString cmd, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err) {
    i32 ret = 0;
    NK_ARENA_SCOPE(scratch) {
        NkStringArray const strs = nks_shell_lex(scratch, cmd);

        char const **args = nk_arena_allocTn(scratch, char const *, strs.size + 1);
        NK_ITERATE(NkString const *, it, strs) {
            args[NK_INDEX(it, strs)] = it->data;
        }
        args[strs.size] = NULL;

        ret = execAsyncImpl(args, process, in, out, err);
    }
    return ret;
}

i32 nk_waitProc(NkHandle process, i32 *exit_status) {
    if (!nk_handleIsNull(process)) {
        for (;;) {
            i32 wstatus = 0;
            if (waitpid(handle2pid(process), &wstatus, 0) < 0) {
                return -1;
            }

            if (WIFEXITED(wstatus)) {
                if (exit_status) {
                    *exit_status = WEXITSTATUS(wstatus);
                }
                return 0;
            }

            if (WIFSIGNALED(wstatus)) {
                if (exit_status) {
                    *exit_status = 128 + WTERMSIG(wstatus);
                }
                return 0;
            }
        }
    } else {
        return 0;
    }
}
