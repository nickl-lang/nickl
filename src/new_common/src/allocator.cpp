#include "nk/common/allocator.h"

#include <cstdlib>

namespace {

void *defaultAllocate(NkAllocator *, size_t size) {
    return std::malloc(size);
}

void defaultFree(NkAllocator *, void *ptr) {
    std::free(ptr);
}

} // namespace

static NkAllocator s_default_allocator = {
    .allocate = defaultAllocate,
    .free = defaultFree,
};

NkAllocator *nk_default_allocator = &s_default_allocator;
