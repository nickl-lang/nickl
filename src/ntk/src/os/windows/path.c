#include "ntk/os/path.h"

#include <stdlib.h>

#include "common.h"
#include "ntk/profiler.h"

char nk_path_separator = '\\';

i32 nk_getBinaryPath(char *buf, usize size) {
    NK_PROF_FUNC_BEGIN();
    DWORD dwBytesWritten = GetModuleFileName(
        NULL, // HMODULE hModule
        buf,  // LPSTR   lpFilename
        size  // DWORD   nSize
    );
    NK_PROF_FUNC_END();
    return dwBytesWritten > 0 ? (i32)dwBytesWritten : -1;
}

i32 nk_fullPath(char *buf, char const *path) {
    if (_fullpath(buf, path, NK_MAX_PATH)) {
        return 0;
    } else {
        return -1;
    }
}
