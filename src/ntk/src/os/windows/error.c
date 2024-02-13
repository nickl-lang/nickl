#include "ntk/os/error.h"

#include "common.h"

nkerr_t nk_getLastError(void) {
    return GetLastError();
}

void nk_setLastError(nkerr_t err) {
    SetLastError(err);
}

char const *nk_getErrorString(nkerr_t err) {
    _Thread_local static char buf[1024];
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // DWORD   dwFlags
        NULL,                                                       // LPCVOID lpSource
        err,                                                        // DWORD   dwMessageId
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                  // DWORD   dwLanguageId
        buf,                                                        // LPTSTR  lpBuffer
        sizeof(buf),                                                // DWORD   nSize
        NULL                                                        // va_list *Arguments
    );

    return buf;
}
