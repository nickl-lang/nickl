#include "nk/common/allocator.h"

#include <cassert>
#include <cstdlib>

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(mem);

void *defaultAllocatorProc(void *, NkAllocatorMode mode, size_t size, uint8_t, void *old_mem, size_t) {
    switch (mode) {
    case NkAllocator_Alloc:
        NK_LOG_TRC("malloc(%" PRIu64 ")", size);
        return std::malloc(size);

    case NkAllocator_Free:
        NK_LOG_TRC("free(%p)", old_mem);
        std::free(old_mem);
        return nullptr;

    case NkAllocator_Realloc:
        NK_LOG_TRC("realloc(%" PRIu64 ", %p)", size, old_mem);
        return std::realloc(old_mem, size);

    case NkAllocator_QuerySpaceLeft:
        *(NkAllocatorSpaceLeftQueryResult *)old_mem = {
            .kind = NkAllocatorSpaceLeft_Unknown,
            .bytes_left = 0,
        };
        return nullptr;

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
