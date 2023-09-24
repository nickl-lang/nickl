#include "nk/sys/file.h"

#include <unistd.h>

int nk_read(nkfd_t fd, char *buf, size_t n) {
    return read(fd, buf, n);
}

int nk_write(nkfd_t fd, char const *buf, size_t n) {
    return write(fd, buf, n);
}

int nk_close(nkfd_t fd) {
    return close(fd);
}
