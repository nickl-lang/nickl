#include "nk/sys/file.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

nkfd_t nk_invalidFd(void) {
    return 0;
}

int nk_read(nkfd_t fd, char *buf, size_t n) {
    DWORD nNumberOfBytesRead = 0;
    BOOL bSuccess = ReadFile(
        (HANDLE)fd,          // HANDLE       hFile
        buf,                 // LPVOID       lpBuffer
        n,                   // DWORD        nNumberOfBytesToRead
        &nNumberOfBytesRead, // LPDWORD      lpNumberOfBytesRead
        NULL                 // LPOVERLAPPED lpOverlapped
    );

    if (!bSuccess) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING && err != ERROR_BROKEN_PIPE) {
            return -1;
        }
    }

    return nNumberOfBytesRead;
}

int nk_write(nkfd_t fd, char const *buf, size_t n) {
    DWORD nNumberOfBytesWritten = 0;
    BOOL bSuccess = WriteFile(
        (HANDLE)fd,             // HANDLE       hFile
        buf,                    // LPCVOID      lpBuffer
        n,                      // DWORD        nNumberOfBytesToWrite
        &nNumberOfBytesWritten, // LPDWORD      lpNumberOfBytesWritten
        NULL                    // LPOVERLAPPED lpOverlapped
    );

    return bSuccess ? (int)nNumberOfBytesWritten : -1;
}

nkfd_t nk_openNullFile(void) {
    HANDLE hFile = CreateFile(
        "nul",                              // LPCSTR                lpFileName,
        GENERIC_READ | GENERIC_WRITE,       // DWORD                 dwDesiredAccess,
        FILE_SHARE_READ | FILE_SHARE_WRITE, // DWORD                 dwShareMode,
        NULL,                               // LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        OPEN_EXISTING,                      // DWORD                 dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL,              // DWORD                 dwFlagsAndAttributes,
        NULL                                // HANDLE                hTemplateFile
    );
    return (nkfd_t)hFile;
}

int nk_close(nkfd_t fd) {
    return CloseHandle((HANDLE)fd) ? 0 : -1;
}
