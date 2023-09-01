#include <unistd.h>

#define WIN32_LEAN_AND_MEAN
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

void *nk_valloc(size_t len) {
    return VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void nk_vfree(void *addr, size_t) {
    VirtualFree(addr, 0, MEM_RELEASE);
}
