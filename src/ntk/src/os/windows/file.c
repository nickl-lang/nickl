#include "ntk/file.h"

#include "common.h"
#include "ntk/profiler.h"

char const *nk_null_file = "nul";

i32 nk_read(NkHandle h_file, char *buf, usize n) {
    NK_PROF_FUNC_BEGIN();

    DWORD nNumberOfBytesRead = 0;
    BOOL bSuccess = ReadFile(
        handle_toNative(h_file), // HANDLE       hFile
        buf,                     // LPVOID       lpBuffer
        n,                       // DWORD        nNumberOfBytesToRead
        &nNumberOfBytesRead,     // LPDWORD      lpNumberOfBytesRead
        NULL                     // LPOVERLAPPED lpOverlapped
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

i32 nk_write(NkHandle h_file, char const *buf, usize n) {
    NK_PROF_FUNC_BEGIN();

    DWORD nNumberOfBytesWritten = 0;
    BOOL bSuccess = WriteFile(
        handle_toNative(h_file), // HANDLE       hFile
        buf,                     // LPCVOID      lpBuffer
        n,                       // DWORD        nNumberOfBytesToWrite
        &nNumberOfBytesWritten,  // LPDWORD      lpNumberOfBytesWritten
        NULL                     // LPOVERLAPPED lpOverlapped
    );

    NK_PROF_FUNC_END();
    return bSuccess ? (i32)nNumberOfBytesWritten : -1;
}

NkHandle nk_open(char const *file, i32 flags) {
    NK_PROF_FUNC_BEGIN();

    NkHandle h_file = NK_HANDLE_ZERO;

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
    if (hFile != INVALID_HANDLE_VALUE) {
        h_file = handle_fromNative(hFile);
    }

    NK_PROF_FUNC_END();
    return h_file;
}

i32 nk_close(NkHandle h_file) {
    if (!nk_handleIsZero(h_file)) {
        NK_PROF_FUNC_BEGIN();
        i32 ret = CloseHandle(handle_toNative(h_file)) ? 0 : -1;
        NK_PROF_FUNC_END();
        return ret;
    } else {
        return 0;
    }
}

NkHandle nk_stdin() {
    return handle_fromNative(GetStdHandle(STD_INPUT_HANDLE));
}

NkHandle nk_stdout() {
    return handle_fromNative(GetStdHandle(STD_OUTPUT_HANDLE));
}

NkHandle nk_stderr() {
    return handle_fromNative(GetStdHandle(STD_ERROR_HANDLE));
}
