#include "nk/mem/stack_allocator.hpp"

namespace nk {

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

size_t StackAllocator::size() const {
    return m_arena.size;
}

void StackAllocator::clear() {
    m_arena.clear();
}

} // namespace nk