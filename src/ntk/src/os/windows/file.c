#include "ntk/file.h"

#include "common.h"
#include "ntk/profiler.h"

char const *nk_null_file = "nul";

i32 nk_read(NkHandle file, char *buf, usize n) {
    i32 ret = 0;
    NK_PROF_FUNC() {
        DWORD nNumberOfBytesRead = 0;
        BOOL bSuccess = ReadFile(
            handle2native(file), // HANDLE       hFile
            buf,                 // LPVOID       lpBuffer
            n,                   // DWORD        nNumberOfBytesToRead
            &nNumberOfBytesRead, // LPDWORD      lpNumberOfBytesRead
            NULL                 // LPOVERLAPPED lpOverlapped
        );
        ret = nNumberOfBytesRead;

        if (!bSuccess) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING && err != ERROR_BROKEN_PIPE) {
                ret = -1;
            }
        }
    }
    return ret;
}

i32 nk_write(NkHandle file, char const *buf, usize n) {
    i32 ret;
    NK_PROF_FUNC() {
        DWORD nNumberOfBytesWritten = 0;
        BOOL bSuccess = WriteFile(
            handle2native(file),    // HANDLE       hFile
            buf,                    // LPCVOID      lpBuffer
            n,                      // DWORD        nNumberOfBytesToWrite
            &nNumberOfBytesWritten, // LPDWORD      lpNumberOfBytesWritten
            NULL                    // LPOVERLAPPED lpOverlapped
        );

        ret = bSuccess ? (i32)nNumberOfBytesWritten : -1;
    }
    return ret;
}

i32 nk_flush(NkHandle file) {
    i32 ret;
    NK_PROF_FUNC() {
        BOOL bSuccess = FlushFileBuffers(handle2native(file) //  HANDLE hFile
        );
        ret = bSuccess ? 0 : -1;
    }
    return ret;
}

NkHandle nk_open(char const *path, i32 flags) {
    NkHandle file = NK_NULL_HANDLE;
    NK_PROF_FUNC() {
        DWORD dwDesiredAccess =
            ((flags & NkOpenFlags_Read) ? GENERIC_READ : 0) | ((flags & NkOpenFlags_Write) ? GENERIC_WRITE : 0);
        DWORD dwShareMode =
            ((flags & NkOpenFlags_Read) ? FILE_SHARE_READ : 0) | ((flags & NkOpenFlags_Write) ? FILE_SHARE_WRITE : 0);
        DWORD dwCreationDisposition = (flags & NkOpenFlags_Create)     ? CREATE_ALWAYS
                                      : (flags & NkOpenFlags_Truncate) ? TRUNCATE_EXISTING
                                                                       : OPEN_EXISTING;
        HANDLE hFile = CreateFile(
            path,                  // LPCSTR                lpFileName
            dwDesiredAccess,       // DWORD                 dwDesiredAccess
            dwShareMode,           // DWORD                 dwShareMode
            NULL,                  // LPSECURITY_ATTRIBUTES lpSecurityAttributes
            dwCreationDisposition, // DWORD                 dwCreationDisposition
            FILE_ATTRIBUTE_NORMAL, // DWORD                 dwFlagsAndAttributes
            NULL                   // HANDLE                hTemplateFile
        );
        if (hFile != INVALID_HANDLE_VALUE) {
            file = native2handle(hFile);
        }
    }
    return file;
}

i32 nk_close(NkHandle file) {
    i32 ret = 0;
    NK_PROF_FUNC() {
        if (!nk_handleIsNull(file)) {
            ret = CloseHandle(handle2native(file)) ? 0 : -1;
        }
    }
    return ret;
}

NkHandle nk_stdin() {
    return native2handle(GetStdHandle(STD_INPUT_HANDLE));
}

NkHandle nk_stdout() {
    return native2handle(GetStdHandle(STD_OUTPUT_HANDLE));
}

NkHandle nk_stderr() {
    return native2handle(GetStdHandle(STD_ERROR_HANDLE));
}
