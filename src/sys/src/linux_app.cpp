#include "nk/sys/app.hpp"

namespace fs = std::filesystem;

fs::path nksys_appDir() {
    return fs::canonical(fs::path("/proc/self/exe")).parent_path();
}
