#include "ntk/os/path.h"

#include <stdlib.h>
#include <unistd.h>

#include "ntk/profiler.h"

char nk_path_separator = '/';

i32 nk_getBinaryPath(char *buf, usize size) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = readlink("/proc/self/exe", buf, size);
    NK_PROF_FUNC_END();
    return ret;
}

i32 nk_fullPath(char *buf, char const *path) {
    if (realpath(path, buf)) {
        return 0;
    } else {
        return -1;
    }
}
