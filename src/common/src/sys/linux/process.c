#include "nk/sys/process.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "nk/common/string.h"

nkpipe_t nk_createPipe(void) {
    nkpipe_t pip = {-1, -1};

    int pipefd[2];
    if (pipe(pipefd) == 0) {
        pip.read = pipefd[0];
        pip.write = pipefd[1];
    }

    return pip;
}

#define MAX_ARGS 31
#define CMD_BUF_SIZE 4095

int nk_execAsync(char const *cmd, nkpid_t *pid, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err) {
    char cmd_buf[CMD_BUF_SIZE + 1];
    size_t cmd_buf_pos = 0;

    char *args[MAX_ARGS + 1];
    size_t argc = 0;

    nkstr cmd_str = nk_mkstr(cmd);
    for (;;) {
        cmd_str = nks_trim_left(cmd_str);
        if (!cmd_str.size) {
            break;
        }

        if (argc == MAX_ARGS) {
            errno = E2BIG;
            return -1;
        }
        args[argc++] = &cmd_buf[cmd_buf_pos];

        nkstr arg = nks_chop_by_delim(&cmd_str, ' ');
        while (arg.size) {
            if (cmd_buf_pos == CMD_BUF_SIZE) {
                errno = E2BIG;
                return -1;
            }
            cmd_buf[cmd_buf_pos++] = nkav_first(arg);
            arg.size--;
            arg.data++;
        }
        cmd_buf[cmd_buf_pos++] = '\0';
    }
    args[argc++] = NULL;

    int err_pipe[2];
    if (pipe(err_pipe) < 0) {
        return -1;
    }

    int error_code = 0;

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

int nk_waitpid(nkpid_t pid, int *exit_status) {
    for (;;) {
        int wstatus = 0;
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

extern inline int nk_execSync(char const *cmd, nkpipe_t *in, nkpipe_t *out, nkpipe_t *err, int *exit_status);
