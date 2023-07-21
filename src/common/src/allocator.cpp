#include "nk/common/allocator.h"

#include <cassert>
#include <cstdlib>

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(mem);

void *defaultAllocatorProc(void * /*data*/, NkAllocatorMode mode, size_t size, void *old_mem, size_t /*old_size*/) {
    switch (mode) {
    case NkAllocator_Alloc:
        NK_LOG_TRC("malloc(%lu)", size);
        return std::malloc(size);

    case NkAllocator_Free:
        NK_LOG_TRC("free(%p)", old_mem);
        std::free(old_mem);
        return nullptr;

    case NkAllocator_Realloc:
        NK_LOG_TRC("realloc(%lu, %p)", size, old_mem);
        return std::realloc(old_mem, size);

    default:
        assert(!"unreachable");
        return nullptr;
    }
}

} // namespace

NkAllocator nk_default_allocator = {
    .data = nullptr,
    .proc = defaultAllocatorProc,
};
