#include "nk/common/mem.h"

#include <cstdint>
#include <cstdlib>

#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"

LOG_USE_SCOPE(mem);

void *platform_alloc(size_t size) {
    EASY_FUNCTION(profiler::colors::Grey200)

    void *mem = std::malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem);
    return mem;
}

void platform_free(void *ptr) {
    EASY_FUNCTION(profiler::colors::Grey200)

    LOG_TRC("free(ptr=%p)", ptr);
    std::free(ptr);
}
