#include "ntk/sys/path.h"

#include <unistd.h>

char nk_path_separator = '/';

int nk_getBinaryPath(char *buf, size_t size) {
    return readlink("/proc/self/exe", buf, size);
}
