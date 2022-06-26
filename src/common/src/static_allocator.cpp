#include "nk/common/static_allocator.hpp"

#include <cassert>

#include "nk/common/utils.h"

namespace nk {

void StaticAllocator::clear() {
    m_size = 0;
}

void *StaticAllocator::alloc_aligned(size_t size, size_t align) {
    uint8_t *data = (uint8_t *)roundUpSafe((size_t)(m_dst.data + m_size), align);
    m_size = (size_t)(data + size - m_dst.data);
    assert(m_size <= m_dst.size && "buffer overflow");
    return data;
}

} // namespace nk
