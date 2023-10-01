#include "ntk/sys/app.hpp"

namespace fs = std::filesystem;

fs::path nk_appDir() {
    return fs::canonical(fs::path("/proc/self/exe")).parent_path();
}
