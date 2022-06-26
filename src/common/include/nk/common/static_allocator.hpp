#ifndef HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR

#include <cstdint>

#include "nk/common/allocator.hpp"

struct StaticAllocator : Allocator {
    StaticAllocator(Slice<uint8_t> dst)
        : m_dst{dst}
        , m_size{} {
    }

    void clear();

protected:
    void *alloc_aligned(size_t size, size_t align) override;

private:
    Slice<uint8_t> m_dst;
    size_t m_size;
};

#endif // HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
