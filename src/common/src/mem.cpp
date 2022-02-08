#include "nk/common/mem.hpp"

#include <cstdint>
#include <cstdlib>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"

LOG_USE_SCOPE(mem)

void *Allocator::alloc_aligned(size_t size, size_t align) {
    EASY_FUNCTION(profiler::colors::Grey200)

    if (size) {
        size_t const extra = (align - 1) + sizeof(void *);
        void *mem = alloc(size + extra);
        void **ptr_array = (void **)(((uintptr_t)mem + extra) & ~(align - 1));
        ptr_array[-1] = mem;
        return ptr_array;
    } else {
        return nullptr;
    }
}

void Allocator::free_aligned(void *ptr) {
    EASY_FUNCTION(profiler::colors::Grey200)

    if (ptr) {
        free(((void **)ptr)[-1]);
    }
}

void *LibcAllocator::alloc(size_t size) {
    EASY_FUNCTION(profiler::colors::Grey200)

    void *mem = std::malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem)
    return mem;
}

void LibcAllocator::free(void *ptr) {
    EASY_FUNCTION(profiler::colors::Grey200)

    LOG_TRC("free(ptr=%p)", ptr)
    std::free(ptr);
}

static LibcAllocator s_libc_allocator;

thread_local MemCtx _mctx{
    .def_allocator = &s_libc_allocator,
    .tmp_allocator = nullptr,
};
