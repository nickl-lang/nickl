#include <unistd.h>
#include <windows.h>

#include "nk/sys/app.hpp"
#include "nk/sys/term.h"

namespace fs = std::filesystem;

fs::path nksys_appDir() {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL, szFileName, MAX_PATH);
    return fs::path{szFileName}.parent_path();
}

bool nksys_isatty() {
    return ::isatty(fileno(stdout));
}
