#ifndef HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR

#include <cstdint>

#include "nk/mem/allocator.hpp"

namespace nk {

//@Todo Make StaticAllocator allocate dynamic memory if needed
struct StaticAllocator : Allocator {
    StaticAllocator(Slice<uint8_t> dst)
        : m_dst{dst}
        , m_size{} {
    }

    void *alloc_aligned(size_t size, size_t align) override;

    Frame pushFrame() override;
    void popFrame(Frame frame) override;

    size_t size() const override;
    void clear() override;

private:
    Slice<uint8_t> m_dst;
    size_t m_size;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STATIC_ALLOCATOR
