#include "ntk/path.h"

#include <unistd.h>

#include "ntk/profiler.h"

i32 nk_getBinaryPath(char *buf, usize size) {
    i32 ret;
    NK_PROF_FUNC() {
        ret = readlink("/proc/self/exe", buf, size);
    }
    return ret;
}
