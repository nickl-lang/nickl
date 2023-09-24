#include "nk/sys/mem.h"

#include "common.h"

void *nk_valloc(size_t len) {
    return VirtualAlloc(
        NULL,                     // LPVOID lpAddress
        len,                      // SIZE_T dwSize
        MEM_RESERVE | MEM_COMMIT, // DWORD  flAllocationType
        PAGE_READWRITE            // DWORD  flProtect
    );
}

int nk_vfree(void *addr, size_t len) {
    (void)len;
    BOOL bSuccess = VirtualFree(
        addr,       // LPVOID lpAddress
        0,          // SIZE_T dwSize
        MEM_RELEASE // DWORD  dwFreeType
    );
    return bSuccess ? 0 : -1;
}
