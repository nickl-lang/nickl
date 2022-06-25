#include "nk/common/stack_allocator.hpp"

void StackAllocator::deinit() {
    m_arena.deinit();
}

void StackAllocator::reserve(size_t size) {
    m_arena.reserve(size);
}

auto StackAllocator::pushFrame() -> Frame {
    return {m_arena.size};
}

void StackAllocator::popFrame(Frame frame) {
    m_arena.pop(m_arena.size - frame.size);
}

void *StackAllocator::alloc_aligned(size_t size, size_t align) {
    return m_arena.push_aligned(align, size).data;
}

void StackAllocator::clear() {
    m_arena.clear();
}
