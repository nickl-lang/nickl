#include "ntk/sys/path.h"

#include <unistd.h>

#include "ntk/profiler.h"

char nk_path_separator = '/';

int nk_getBinaryPath(char *buf, size_t size) {
    ProfBeginFunc();
    int ret = readlink("/proc/self/exe", buf, size);
    ProfEndBlock();
    return ret;
}
