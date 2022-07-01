#ifndef HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"

namespace nk {

struct StackAllocator : Allocator {
    struct Frame {
        size_t size;
    };

    void deinit();

    void reserve(size_t size);

    Frame pushFrame();
    void popFrame(Frame frame);

    void clear();

    size_t size() const;

    void *alloc_aligned(size_t size, size_t align) override;

private:
    Arena<char> m_arena;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
