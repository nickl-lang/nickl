#include "nk/mem/stack_allocator.hpp"

#include "nk/utils/profiler.hpp"

namespace nk {

void StackAllocator::deinit() {
    EASY_BLOCK("StackAllocator::deinit", profiler::colors::Grey200)
    m_arena.deinit();
}

void StackAllocator::reserve(size_t size) {
    EASY_BLOCK("StackAllocator::reserve", profiler::colors::Grey200)
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
