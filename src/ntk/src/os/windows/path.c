#include "ntk/path.h"

#include <shlwapi.h>

#include "common.h"
#include "ntk/profiler.h"

i32 nk_getBinaryPath(char *buf, usize size) {
    i32 ret;
    NK_PROF_FUNC() {
        DWORD dwBytesWritten = GetModuleFileName(
            NULL, // HMODULE hModule
            buf,  // LPSTR   lpFilename
            size  // DWORD   nSize
        );
        ret = dwBytesWritten > 0 ? (i32)dwBytesWritten : -1;
    }
    return ret;
}

i32 nk_fullPath(char *buf, char const *path) {
    if (GetFullPathNameA(
           path,        // LPCSTR lpFileName,
           NK_MAX_PATH, // DWORD  nBufferLength,
           buf,         // LPSTR  lpBuffer,
           NULL         // LPSTR  *lpFilePart
    )) {
        return 0;
    } else {
        return -1;
    }
}

i32 nk_getCwd(char *buf, usize size) {
    return GetCurrentDirectory(
        size, // DWORD  nBufferLength,
        buf   // LPTSTR lpBuffer
    ) ? 0 : -1;
}

bool nk_pathIsRelative(char const *path) {
    return PathIsRelativeA(path);
}
