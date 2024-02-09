#include "ntk/sys/path.h"

#include <unistd.h>

#include "ntk/profiler.h"

char nk_path_separator = '/';

i32 nk_getBinaryPath(char *buf, usize size) {
    ProfBeginFunc();
    i32 ret = readlink("/proc/self/exe", buf, size);
    ProfEndBlock();
    return ret;
}
