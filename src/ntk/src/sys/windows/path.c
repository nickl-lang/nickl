#include "ntk/sys/path.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char nk_path_separator = '\\';

int nk_getBinaryPath(char *buf, size_t size) {
    DWORD dwBytesWritten = GetModuleFileName(
        NULL, // HMODULE hModule
        buf,  // LPSTR   lpFilename
        size  // DWORD   nSize
    );
    return dwBytesWritten > 0 ? (int)dwBytesWritten : -1;
}
