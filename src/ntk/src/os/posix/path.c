#include "ntk/path.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

i32 nk_fullPath(char *buf, char const *path) {
    if (realpath(path, buf)) {
        return 0;
    } else {
        return -1;
    }
}

i32 nk_getCwd(char *buf, usize size) {
    return getcwd(buf, size) ? 0 : -1;
}

bool nk_pathIsRelative(char const *path) {
    return !strlen(path) || path[0] != NK_PATH_SEPARATOR;
}
