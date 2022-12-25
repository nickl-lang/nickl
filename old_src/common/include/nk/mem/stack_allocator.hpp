#ifndef HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR

#include "nk/ds/arena.hpp"
#include "nk/mem/allocator.hpp"

namespace nk {

struct StackAllocator : Allocator {
    void deinit();

    void reserve(size_t size);

    void *alloc_aligned(size_t size, size_t align) override;

    Frame pushFrame() override;
    void popFrame(Frame frame) override;

    size_t size() const override;
    void clear() override;

private:
    Arena<char> m_arena;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
