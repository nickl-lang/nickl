#include "nk/common/mem.hpp"

#include <cstdint>
#include <cstdlib>

#include "nk/common/sequence.hpp"
#include "nk/common/logger.hpp"

LOG_USE_SCOPE(mem)

void *lang_malloc(size_t size) {
    void *mem = std::malloc(size);
    LOG_TRC("malloc(size=%lu) -> %p", size, mem)
    return mem;
}

void *lang_realloc(void *ptr, size_t new_size) {
    void *mem = std::realloc(ptr, new_size);
    LOG_TRC("realloc(ptr=%p, new_size=%lu) -> %p", ptr, new_size, mem)
    return mem;
}

void lang_free(void *ptr) {
    LOG_TRC("free(ptr=%p)", ptr)
    std::free(ptr);
}

void *Allocator::alloc_aligned(size_t size, size_t align) {
    size_t const extra = (align - 1) + sizeof(void *);
    void *mem = alloc(size + extra);
    void **ptr_array = (void **)(((uintptr_t)mem + extra) & ~(align - 1));
    ptr_array[-1] = mem;
    return ptr_array;
}

void Allocator::free_aligned(void *ptr) {
    if (ptr) {
        free(((void **)ptr)[-1]);
    }
}

void *LibcAllocator::alloc(size_t size) {
    return lang_malloc(size);
}

void LibcAllocator::free(void *ptr) {
    lang_free(ptr);
}
