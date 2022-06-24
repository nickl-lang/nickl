#ifndef HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
#define HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"

struct StackAllocator : Allocator {
    struct Frame {
        size_t size;
    };

    void deinit();

    void reserve(size_t size);

    Frame pushFrame();
    void popFrame(Frame frame);

protected:
    void *alloc_aligned(size_t size, size_t align);

private:
    Arena<char> m_arena;
};

#endif // HEADER_GUARD_NK_COMMON_STACK_ALLOCATOR
