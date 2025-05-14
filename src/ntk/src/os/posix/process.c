#include "ntk/process.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>

#include "common.h"
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

#define MAX_ARGS 31
#define CMD_BUF_SIZE 4095

i32 nk_execAsync(char const *cmd, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err) {
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
