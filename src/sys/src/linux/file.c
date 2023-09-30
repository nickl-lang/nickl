#include "nk/sys/file.h"

#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

nkfd_t nk_invalidFd(void) {
    return -1;
}

int nk_read(nkfd_t fd, char *buf, size_t n) {
    return read(fd, buf, n);
}

int nk_write(nkfd_t fd, char const *buf, size_t n) {
    return write(fd, buf, n);
}

nkfd_t nk_openNullFile(void) {
    return open("/dev/null", O_RDWR);
}

int nk_close(nkfd_t fd) {
    return close(fd);
}
