#include "ntk/os/file.h"

#include "ntk/profiler.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

nkfd_t nk_invalid_fd = 0;
char const *nk_null_file = "nul";

i32 nk_read(nkfd_t fd, char *buf, usize n) {
    NK_PROF_FUNC_BEGIN();

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
            NK_PROF_FUNC_END();
            return -1;
        }
    }

    NK_PROF_FUNC_END();
    return nNumberOfBytesRead;
}

i32 nk_write(nkfd_t fd, char const *buf, usize n) {
    NK_PROF_FUNC_BEGIN();

    DWORD nNumberOfBytesWritten = 0;
    BOOL bSuccess = WriteFile(
        (HANDLE)fd,             // HANDLE       hFile
        buf,                    // LPCVOID      lpBuffer
        n,                      // DWORD        nNumberOfBytesToWrite
        &nNumberOfBytesWritten, // LPDWORD      lpNumberOfBytesWritten
        NULL                    // LPOVERLAPPED lpOverlapped
    );

    NK_PROF_FUNC_END();
    return bSuccess ? (i32)nNumberOfBytesWritten : -1;
}

nkfd_t nk_open(char const *file, i32 flags) {
    NK_PROF_FUNC_BEGIN();

    DWORD dwDesiredAccess =
        ((flags & NkOpenFlags_Read) ? GENERIC_READ : 0) | ((flags & NkOpenFlags_Write) ? GENERIC_WRITE : 0);
    DWORD dwShareMode =
        ((flags & NkOpenFlags_Read) ? FILE_SHARE_READ : 0) | ((flags & NkOpenFlags_Write) ? FILE_SHARE_WRITE : 0);
    DWORD dwCreationDisposition = (flags & NkOpenFlags_Create)     ? CREATE_ALWAYS
                                  : (flags & NkOpenFlags_Truncate) ? TRUNCATE_EXISTING
                                                                   : OPEN_EXISTING;
    HANDLE hFile = CreateFile(
        file,                  // LPCSTR                lpFileName,
        dwDesiredAccess,       // DWORD                 dwDesiredAccess,
        dwShareMode,           // DWORD                 dwShareMode,
        NULL,                  // LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        dwCreationDisposition, // DWORD                 dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL, // DWORD                 dwFlagsAndAttributes,
        NULL                   // HANDLE                hTemplateFile
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        NK_PROF_FUNC_END();
        return nk_invalid_fd;
    } else {
        NK_PROF_FUNC_END();
        return (nkfd_t)hFile;
    }
}

i32 nk_close(nkfd_t fd) {
    NK_PROF_FUNC_BEGIN();
    i32 ret = CloseHandle((HANDLE)fd) ? 0 : -1;
    NK_PROF_FUNC_END();
    return ret;
}

nkfd_t nk_stdin() {
    return (nkfd_t)GetStdHandle(STD_INPUT_HANDLE);
}

nkfd_t nk_stdout() {
    return (nkfd_t)GetStdHandle(STD_OUTPUT_HANDLE);
}

nkfd_t nk_stderr() {
    return (nkfd_t)GetStdHandle(STD_ERROR_HANDLE);
}
