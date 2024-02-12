#include "ntk/os/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ntk/profiler.h"

nkfd_t nk_invalid_fd = -1;
char const *nk_null_file = "/dev/null";

i32 nk_read(nkfd_t fd, char *buf, usize n) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = read(fd, buf, n);
    NK_PROF_FUNC_END();
    return ret;
}

i32 nk_write(nkfd_t fd, char const *buf, usize n) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = write(fd, buf, n);
    NK_PROF_FUNC_END();
    return ret;
}

nkfd_t nk_open(char const *file, i32 flags) {
    NK_PROF_FUNC_BEGIN();
    i32 oflag = ((flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_RDWR : 0) |
                ((flags & NkOpenFlags_Read) && !(flags & NkOpenFlags_Write) ? O_RDONLY : 0) |
                (!(flags & NkOpenFlags_Read) && (flags & NkOpenFlags_Write) ? O_WRONLY : 0) |
                ((flags & NkOpenFlags_Create) ? O_CREAT : 0) | ((flags & NkOpenFlags_Truncate) ? O_TRUNC : 0);
    nkfd_t ret = open(file, oflag, 0644);
    NK_PROF_FUNC_END();
    return ret;
}

i32 nk_close(nkfd_t fd) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = close(fd);
    NK_PROF_FUNC_END();
    return ret;
}

nkfd_t nk_stdin() {
    return 0;
}

nkfd_t nk_stdout() {
    return 1;
}

nkfd_t nk_stderr() {
    return 2;
}
