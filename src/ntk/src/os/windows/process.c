#include "ntk/os/process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

NkPipe nk_proc_createPipe(void) {
    NkPipe pip = {-1, -1};

    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (CreatePipe(
            &hRead,  // PHANDLE               hReadPipe
            &hWrite, // PHANDLE               hWritePipe
            &saAttr, // LPSECURITY_ATTRIBUTES lpPipeAttributes
            0        // DWORD                 nSize
            )) {
        pip.read = (nkfd_t)hRead;
        pip.write = (nkfd_t)hWrite;
    }

    return pip;
}

void nk_proc_closePipe(NkPipe pipe) {
    nk_close(pipe.read);
    nk_close(pipe.write);
}

i32 nk_proc_execAsync(char const *cmd, nkpid_t *pid, NkPipe *in, NkPipe *out, NkPipe *err) {
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput = (in && in->read) ? (HANDLE)in->read : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.hStdOutput = (out && out->write) ? (HANDLE)out->write : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdError = (err && err->write) ? (HANDLE)err->write : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    if (in && in->write) {
        if (!SetHandleInformation((HANDLE)in->write, HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (out && out->read) {
        if (!SetHandleInformation((HANDLE)out->read, HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (err && err->read) {
        if (!SetHandleInformation((HANDLE)err->read, HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    BOOL bSuccess = CreateProcess(
        NULL,         // LPCSTR                lpApplicationName
        (char *)cmd,  // LPSTR                 lpCommandLine
        NULL,         // LPSECURITY_ATTRIBUTES lpProcessAttributes
        NULL,         // LPSECURITY_ATTRIBUTES lpThreadAttributes
        TRUE,         // BOOL                  bInheritHandles
        0,            // DWORD                 dwCreationFlags
        NULL,         // LPVOID                lpEnvironment
        NULL,         // LPCSTR                lpCurrentDirectory
        &siStartInfo, // LPSTARTUPINFOA        lpStartupInfo
        &piProcInfo   // LPPROCESS_INFORMATION lpProcessInformation
    );

    if (in && in->read) {
        CloseHandle((HANDLE)in->read);
    }

    if (out && out->write) {
        CloseHandle((HANDLE)out->write);
    }

    if (err && err->write) {
        CloseHandle((HANDLE)err->write);
    }

    CloseHandle(piProcInfo.hThread);

    if (!bSuccess) {
        return -1;
    }

    *pid = (nkpid_t)piProcInfo.hProcess;

    return 0;
}

i32 nk_proc_waitpid(nkpid_t pid, i32 *exit_status) {
    DWORD dwResult = WaitForSingleObject(
        (HANDLE)pid, // HANDLE hHandle,
        INFINITE     // DWORD  dwMilliseconds
    );

    if (dwResult == WAIT_FAILED) {
        return -1;
    }

    if (exit_status) {
        DWORD dwExitCode = 1;
        GetExitCodeProcess(
            (HANDLE)pid, // HANDLE  hProcess,
            &dwExitCode  // LPDWORD lpExitCode
        );
        *exit_status = dwExitCode;
    }

    CloseHandle((HANDLE)pid);

    return 0;
}

extern inline i32 nk_proc_execSync(char const *cmd, NkPipe *in, NkPipe *out, NkPipe *err, i32 *exit_status);