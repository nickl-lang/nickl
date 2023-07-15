#include <unistd.h>
#include <windows.h>

#include "nk/sys/app.hpp"
#include "nk/sys/mem.h"
#include "nk/sys/term.h"

namespace fs = std::filesystem;

fs::path nk_appDir() {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL, szFileName, MAX_PATH);
    return fs::path{szFileName}.parent_path();
}

bool nk_isatty() {
    return ::isatty(fileno(stdout));
}

void *nk_mmap(size_t len) {
    return VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void nk_munmap(void *addr, size_t len) {
    VirtualFree(addr, 0, MEM_RELEASE);
}
