#include "ntk/sys/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ntk/profiler.h"

nkfd_t nk_invalid_fd = -1;
char const *nk_null_file = "/dev/null";

i32 nk_read(nkfd_t fd, char *buf, usize n) {
    ProfBeginFunc();
    i32 ret = read(fd, buf, n);
    ProfEndBlock();
    return ret;
}

i32 nk_write(nkfd_t fd, char const *buf, usize n) {
    ProfBeginFunc();
    i32 ret = write(fd, buf, n);
    ProfEndBlock();
    return ret;
}

nkfd_t nk_open(char const *file, i32 flags) {
    ProfBeginFunc();
    i32 oflag = ((flags & nk_open_read) && (flags & nk_open_write) ? O_RDWR : 0) |
                ((flags & nk_open_read) && !(flags & nk_open_write) ? O_RDONLY : 0) |
                (!(flags & nk_open_read) && (flags & nk_open_write) ? O_WRONLY : 0) |
                ((flags & nk_open_create) ? O_CREAT : 0) | ((flags & nk_open_truncate) ? O_TRUNC : 0);
    nkfd_t ret = open(file, oflag, 0644);
    ProfEndBlock();
    return ret;
}

i32 nk_close(nkfd_t fd) {
    ProfBeginFunc();
    i32 ret = close(fd);
    ProfEndBlock();
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
