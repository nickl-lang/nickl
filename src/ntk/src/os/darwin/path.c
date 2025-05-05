#include "ntk/path.h"

#include <mach-o/dyld.h>

#include "ntk/profiler.h"

i32 nk_getBinaryPath(char *buf, usize size) {
    i32 ret = -1;
    NK_PROF_FUNC() {
        u32 size32 = size;
        if (_NSGetExecutablePath(buf, &size32) == 0) {
            ret = strlen(buf);
        }
    }
    return ret;
}
