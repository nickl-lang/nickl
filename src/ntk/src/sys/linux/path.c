#include "ntk/sys/path.h"

#include <unistd.h>

int nk_getBinaryPath(char *buf, size_t size) {
    return readlink("/proc/self/exe", buf, size);
}
