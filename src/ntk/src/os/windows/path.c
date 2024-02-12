#include "ntk/os/path.h"

#include "ntk/profiler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
