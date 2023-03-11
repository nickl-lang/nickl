#include <unistd.h>

#include "nk/sys/app.hpp"
#include "nk/sys/tty.h"

namespace fs = std::filesystem;

fs::path nksys_appDir() {
    return fs::canonical(fs::path("/proc/self/exe")).parent_path();
}

bool nksys_isatty() {
    return ::isatty(fileno(stdout));
}
