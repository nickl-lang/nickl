#include <unistd.h>

#include <sys/mman.h>

#include "nk/sys/app.hpp"
#include "nk/sys/mem.h"
#include "nk/sys/term.h"

namespace fs = std::filesystem;

fs::path nk_appDir() {
    return fs::canonical(fs::path("/proc/self/exe")).parent_path();
}

bool nk_isatty() {
    return ::isatty(fileno(stdout));
}

void *nk_valloc(size_t len) {
    return ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void nk_vfree(void *addr, size_t len) {
    ::munmap(addr, len);
}
