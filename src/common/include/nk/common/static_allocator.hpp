#ifndef HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR

#include <cstdint>

#include "nk/common/allocator.hpp"

namespace nk {

struct StaticAllocator : Allocator {
    StaticAllocator(Slice<uint8_t> dst)
        : m_dst{dst}
        , m_size{} {
    }

    void clear();

    void *alloc_aligned(size_t size, size_t align) override;

private:
    Slice<uint8_t> m_dst;
    size_t m_size;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
