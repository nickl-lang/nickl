#include "ntk/os/path.h"

#include <unistd.h>

#include "ntk/profiler.h"

i32 nk_getBinaryPath(char *buf, usize size) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = readlink("/proc/self/exe", buf, size);
    NK_PROF_FUNC_END();
    return ret;
}
