#include "nk/sys/file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

nkfd_t nk_invalid_fd = -1;
char const *nk_null_file = "/dev/null";

int nk_read(nkfd_t fd, char *buf, size_t n) {
    return read(fd, buf, n);
}

int nk_write(nkfd_t fd, char const *buf, size_t n) {
    return write(fd, buf, n);
}

nkfd_t nk_open(char const *file, int flags) {
    int oflag = ((flags & nk_open_read) && (flags & nk_open_write) ? O_RDWR : 0) |
                ((flags & nk_open_read) && !(flags & nk_open_write) ? O_RDONLY : 0) |
                (!(flags & nk_open_read) && (flags & nk_open_write) ? O_WRONLY : 0) |
                ((flags & nk_open_create) ? O_CREAT : 0) | ((flags & nk_open_truncate) ? O_TRUNC : 0);
    return open(file, oflag, 0644);
}

int nk_close(nkfd_t fd) {
    return close(fd);
}
