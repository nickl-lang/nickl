#include "nk/common/allocator.h"

#include <cassert>
#include <cstdlib>

namespace {

void *defaultAllocatorProc(void * /*data*/, NkAllocatorMode mode, size_t size, void *old_mem, size_t /*old_size*/) {
    switch (mode) {
    case NkAllocator_Alloc:
        return std::malloc(size);

    case NkAllocator_Free:
        std::free(old_mem);
        return nullptr;

    case NkAllocator_Realloc:
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
