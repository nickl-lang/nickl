#include "ntk/path.h"

#include <shlwapi.h>

#include "common.h"

i32 nk_getBinaryPath(char *buf, usize size) {
    i32 ret;
    DWORD dwBytesWritten = GetModuleFileName(
        NULL, // HMODULE hModule
        buf,  // LPSTR   lpFilename
        size  // DWORD   nSize
    );
    ret = dwBytesWritten > 0 ? (i32)dwBytesWritten : -1;
    return ret;
}

i32 nk_fullPath(char *buf, char const *path) {
    return GetFullPathNameA(
               path,        // LPCSTR lpFileName
               NK_MAX_PATH, // DWORD  nBufferLength
               buf,         // LPSTR  lpBuffer
               NULL         // LPSTR  *lpFilePart
               )
               ? 0
               : -1;
}

i32 nk_getCwd(char *buf, usize size) {
    return GetCurrentDirectory(
               size, // DWORD  nBufferLength
               buf   // LPTSTR lpBuffer
               )
               ? 0
               : -1;
}

bool nk_pathIsRelative(char const *path) {
    return PathIsRelativeA(path);
}

i32 nk_getTempPath(char *buf, usize size) {
    return GetTempPathA(
               size, // DWORD nBufferLength
               buf   // LPSTR lpBuffer
               )
               ? 0
               : -1;
}
