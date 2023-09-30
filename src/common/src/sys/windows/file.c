#include "nk/sys/file.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

nkfd_t nk_invalid_fd = 0;
char const *nk_null_file = "nul";

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

nkfd_t nk_open(char const *file, nk_open_flags flags) {
    DWORD dwDesiredAccess = ((flags & nk_open_read) ? GENERIC_READ : 0) | ((flags & nk_open_write) ? GENERIC_WRITE : 0);
    DWORD dwShareMode =
        ((flags & nk_open_read) ? FILE_SHARE_READ : 0) | ((flags & nk_open_write) ? FILE_SHARE_WRITE : 0);
    DWORD dwCreationDisposition = (flags & nk_open_create)     ? CREATE_NEW
                                  : (flags & nk_open_truncate) ? TRUNCATE_EXISTING
                                                               : OPEN_EXISTING;
    return (nkfd_t)CreateFile(
        file,                  // LPCSTR                lpFileName,
        dwDesiredAccess,       // DWORD                 dwDesiredAccess,
        dwShareMode,           // DWORD                 dwShareMode,
        NULL,                  // LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        dwCreationDisposition, // DWORD                 dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL, // DWORD                 dwFlagsAndAttributes,
        NULL                   // HANDLE                hTemplateFile
    );
}

int nk_close(nkfd_t fd) {
    return CloseHandle((HANDLE)fd) ? 0 : -1;
}
