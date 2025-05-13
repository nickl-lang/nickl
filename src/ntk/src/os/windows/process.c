#include "ntk/process.h"

#include "common.h"
#include "ntk/file.h"

NkPipe nk_pipe_create(void) {
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
        pip.read_file = native2handle(hRead);
        pip.write_file = native2handle(hWrite);
    }

    return pip;
}

void nk_pipe_close(NkPipe pipe) {
    nk_close(pipe.read_file);
    nk_close(pipe.write_file);
}

i32 nk_execAsync(char const *cmd, NkHandle *process, NkPipe *in, NkPipe *out, NkPipe *err) {
    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdInput =
        (in && !nk_handleIsZero(in->read_file)) ? handle2native(in->read_file) : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.hStdOutput =
        (out && !nk_handleIsZero(out->write_file)) ? handle2native(out->write_file) : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdError =
        (err && !nk_handleIsZero(err->write_file)) ? handle2native(err->write_file) : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    if (in && !nk_handleIsZero(in->write_file)) {
        if (!SetHandleInformation(handle2native(in->write_file), HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (out && !nk_handleIsZero(out->read_file)) {
        if (!SetHandleInformation(handle2native(out->read_file), HANDLE_FLAG_INHERIT, 0)) {
            return -1;
        }
    }

    if (err && !nk_handleIsZero(err->read_file)) {
        if (!SetHandleInformation(handle2native(err->read_file), HANDLE_FLAG_INHERIT, 0)) {
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
        nk_close(in->read_file);
    }

    if (out) {
        nk_close(out->write_file);
    }

    if (err) {
        nk_close(err->write_file);
    }

    CloseHandle(piProcInfo.hThread);

    if (!bSuccess) {
        return -1;
    }

    *process = native2handle(piProcInfo.hProcess);

    return 0;
}

i32 nk_waitProc(NkHandle process, i32 *exit_status) {
    if (!nk_handleIsZero(process)) {
        DWORD dwResult = WaitForSingleObject(
            handle2native(process), // HANDLE hHandle,
            INFINITE                // DWORD  dwMilliseconds
        );

        if (dwResult == WAIT_FAILED) {
            return -1;
        }

        if (exit_status) {
            DWORD dwExitCode = 1;
            GetExitCodeProcess(
                handle2native(process), // HANDLE  hProcess,
                &dwExitCode             // LPDWORD lpExitCode
            );
            *exit_status = dwExitCode;
        }

        nk_close(process);
    }

    return 0;
}
