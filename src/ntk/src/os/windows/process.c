#include "ntk/os/process.h"

#include "common.h"

NkPipe nk_proc_createPipe(void) {
    NkPipe pip = {0};

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
        pip.h_read = handle_fromNative(hRead);
        pip.h_write = handle_fromNative(hWrite);
    }

    return pip;
}

void nk_proc_closePipe(NkPipe pipe) {
    nk_close(pipe.h_read);
    nk_close(pipe.h_write);
}

i32 nk_proc_execAsync(char const *cmd, NkOsHandle *h_process, NkPipe *in, NkPipe *out, NkPipe *err) {
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput =
        (in && !nkos_handleIsZero(in->h_read)) ? handle_toNative(in->h_read) : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.hStdOutput =
        (out && !nkos_handleIsZero(out->h_write)) ? handle_toNative(out->h_write) : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdError =
        (err && !nkos_handleIsZero(err->h_write)) ? handle_toNative(err->h_write) : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    if (in && !nkos_handleIsZero(in->h_write)) {
        if (!SetHandleInformation(handle_toNative(in->h_write), HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (out && !nkos_handleIsZero(out->h_read)) {
        if (!SetHandleInformation(handle_toNative(out->h_read), HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (err && !nkos_handleIsZero(err->h_read)) {
        if (!SetHandleInformation(handle_toNative(err->h_read), HANDLE_FLAG_INHERIT, 0)) {
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

    if (in) {
        nk_close(in->h_read);
    }

    if (out) {
        nk_close(out->h_write);
    }

    if (err) {
        nk_close(err->h_write);
    }

    CloseHandle(piProcInfo.hThread);

    if (!bSuccess) {
        return -1;
    }

    *h_process = handle_fromNative(piProcInfo.hProcess);

    return 0;
}

i32 nk_proc_wait(NkOsHandle h_process, i32 *exit_status) {
    if (!nkos_handleIsZero(h_process)) {
        DWORD dwResult = WaitForSingleObject(
            handle_toNative(h_process), // HANDLE hHandle,
            INFINITE                    // DWORD  dwMilliseconds
        );

        if (dwResult == WAIT_FAILED) {
            return -1;
        }

        if (exit_status) {
            DWORD dwExitCode = 1;
            GetExitCodeProcess(
                handle_toNative(h_process), // HANDLE  hProcess,
                &dwExitCode                 // LPDWORD lpExitCode
            );
            *exit_status = dwExitCode;
        }

        nk_close(h_process);
    }

    return 0;
}
