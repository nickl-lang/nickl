#include "ntk/sys/path.h"

#include "ntk/profiler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char nk_path_separator = '\\';

int nk_getBinaryPath(char *buf, size_t size) {
    ProfBeginFunc();
    DWORD dwBytesWritten = GetModuleFileName(
        NULL, // HMODULE hModule
        buf,  // LPSTR   lpFilename
        size  // DWORD   nSize
    );
    ProfEndBlock();
    return dwBytesWritten > 0 ? (int)dwBytesWritten : -1;
}
