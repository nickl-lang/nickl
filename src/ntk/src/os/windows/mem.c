#include "ntk/mem.h"

#include "common.h"

void *nk_mem_reserveAndCommit(usize len) {
    return VirtualAlloc(
        NULL,                     // LPVOID lpAddress
        len,                      // SIZE_T dwSize
        MEM_RESERVE | MEM_COMMIT, // DWORD  flAllocationType
        PAGE_READWRITE            // DWORD  flProtect
    );
}

i32 nk_mem_release(void *addr, usize len) {
    (void)len;
    BOOL bSuccess = VirtualFree(
        addr,       // LPVOID lpAddress
        0,          // SIZE_T dwSize
        MEM_RELEASE // DWORD  dwFreeType
    );
    return bSuccess ? 0 : -1;
}
