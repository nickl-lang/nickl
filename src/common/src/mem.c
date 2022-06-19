#include "nk/common/mem.h"

#include <stdint.h>
#include <stdlib.h>

#include "nk/common/logger.h"

LOG_USE_SCOPE(mem);

void *platform_alloc(size_t size) {
    void *mem = malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem);
    return mem;
}

void platform_free(void *ptr) {
    LOG_TRC("free(ptr=%p)", ptr);
    free(ptr);
}
