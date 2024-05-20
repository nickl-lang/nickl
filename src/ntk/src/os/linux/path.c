#include "ntk/os/path.h"

#include <stdlib.h>
#include <unistd.h>

#include "ntk/profiler.h"

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

i32 nk_getCwd(char *buf, usize size) {
    return getcwd(buf, size) ? -1 : 0;
}

bool nk_pathIsRelative(char const *path) {
    return !strlen(path) || path[0] != NK_PATH_SEPARATOR;
}
