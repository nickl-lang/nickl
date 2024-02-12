#include "ntk/os/process.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "ntk/string.h"

NkPipe nk_proc_createPipe(void) {
    NkPipe pip = {-1, -1};

    i32 pipefd[2];
    if (pipe(pipefd) == 0) {
        pip.read = pipefd[0];
        pip.write = pipefd[1];
    }

    return pip;
}

void nk_proc_closePipe(NkPipe pipe) {
    nk_close(pipe.read);
    nk_close(pipe.write);
}

#define MAX_ARGS 31
#define CMD_BUF_SIZE 4095

i32 nk_proc_execAsync(char const *cmd, nkpid_t *pid, NkPipe *in, NkPipe *out, NkPipe *err) {
    char cmd_buf[CMD_BUF_SIZE + 1];
    usize cmd_buf_pos = 0;

    char *args[MAX_ARGS + 1];
    usize argc = 0;

    NkString cmd_str = nk_cs2s(cmd);
    for (;;) {
        cmd_str = nks_trimLeft(cmd_str);
        if (!cmd_str.size) {
            break;
        }

        if (argc == MAX_ARGS) {
            errno = E2BIG;
            return -1;
        }
        args[argc++] = &cmd_buf[cmd_buf_pos];

        NkString arg = nks_chopByDelim(&cmd_str, ' ');
        while (arg.size) {
            if (cmd_buf_pos == CMD_BUF_SIZE) {
                errno = E2BIG;
                return -1;
            }
            cmd_buf[cmd_buf_pos++] = nks_first(arg);
            arg.size--;
            arg.data++;
        }
        cmd_buf[cmd_buf_pos++] = '\0';
    }
    args[argc++] = NULL;

    i32 err_pipe[2];
    if (pipe(err_pipe) < 0) {
        return -1;
    }

    i32 error_code = 0;

    switch (*pid = fork()) {
    case -1:
        return -1;

    case 0:
        close(err_pipe[0]);
        fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);

        if (in) {
            if (in->read >= 0 && dup2(in->read, STDIN_FILENO) < 0) {
                goto error;
            }
            close(in->write);
        }

        if (out) {
            if (out->write >= 0 && dup2(out->write, STDOUT_FILENO) < 0) {
                goto error;
            }
            close(out->read);
        }

        if (err) {
            if (err->write >= 0 && dup2(err->write, STDERR_FILENO) < 0) {
                goto error;
            }
            close(err->read);
        }

        execvp(args[0], (char *const *)args);

    error:
        error_code = errno;
        (void)!write(err_pipe[1], &error_code, sizeof(error_code));
        _exit(EX_OSERR);

    default:
        if (in && in->read >= 0) {
            close(in->read);
        }

        if (out && out->write >= 0) {
            close(out->write);
        }

        if (err && err->write >= 0) {
            close(err->write);
        }

        close(err_pipe[1]);
        (void)!read(err_pipe[0], &error_code, sizeof(error_code));
        close(err_pipe[0]);
        errno = error_code;

        return errno ? -1 : 0;
    }
}

i32 nk_proc_waitpid(nkpid_t pid, i32 *exit_status) {
    for (;;) {
        i32 wstatus = 0;
        if (waitpid(pid, &wstatus, 0) < 0) {
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
}

extern inline i32 nk_proc_execSync(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status);
