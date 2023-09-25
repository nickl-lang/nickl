#include "nk/sys/app.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fs = std::filesystem;

fs::path nk_appDir() {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL, szFileName, MAX_PATH);
    return fs::path{szFileName}.parent_path();
}
