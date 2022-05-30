#include "nk/common/string_builder.hpp"

#include <cstdarg>

#include "nk/common/utils.hpp"

static constexpr size_t PRINTF_BUFFERS_SIZE = 4096;

struct StringBuilder::BlockHeader {
    BlockHeader *next;
    size_t size;
    size_t capacity;
};

void StringBuilder::reserve(size_t capacity) {
    if (!m_last_block || capacity > m_last_block->capacity - m_last_block->size) {
        _allocateBlock(capacity);
    }
}

int StringBuilder::printf(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[PRINTF_BUFFERS_SIZE];
    int n = vsnprintf(buf, PRINTF_BUFFERS_SIZE, fmt, ap);
    va_end(ap);

    if (n >= (int)PRINTF_BUFFERS_SIZE) {
        va_list ap;
        va_start(ap, fmt);
        char *buf = (char *)::malloc(n + 1);
        defer {
            ::free(buf);
        };
        int n2 = vsnprintf(buf, n + 1, fmt, ap);
        va_end(ap);

        if (n2 >= 0) {
            void *mem = _push(n + 1);
            ::memcpy(mem, buf, n + 1);
        } else {
            return n2;
        }
    } else if (n >= 0) {
        void *mem = _push(n + 1);
        ::memcpy(mem, buf, n + 1);
    } else {
        return n;
    }

    return n;
}

string StringBuilder::moveStr(Allocator &allocator) {
    void *mem = allocator.alloc(m_size);
    for (BlockHeader *block = m_first_block; block; block = block->next) {
        ::memcpy(mem, _blockData(block), block->size);
    }
    for (BlockHeader *block = m_first_block; block;) {
        void *cur_block = block;
        block = block->next;
        ::free(cur_block);
    }
    return {(char const *)mem, m_size - 1};
}

uint8_t *StringBuilder::_push(size_t n) {
    reserve(n);
    m_last_block->size += n;
    m_size += n;
    return _blockData(m_last_block) + m_last_block->size - n;
}

void StringBuilder::_allocateBlock(size_t capacity) {
    auto const header_size = _align(sizeof(BlockHeader));
    capacity = ceilToPowerOf2(capacity + header_size);
    capacity = maxu(capacity, (m_last_block ? m_last_block->capacity << 1 : 0));

    BlockHeader *block = (BlockHeader *)::malloc(capacity);

    block->next = nullptr;
    block->size = 0;
    block->capacity = capacity - header_size;

    if (!m_last_block) {
        m_first_block = block;
        m_last_block = block;
    } else {
        m_last_block->next = block;
    }

    m_last_block = block;
}

uint8_t *StringBuilder::_blockData(BlockHeader *block) const {
    return (uint8_t *)_align((size_t)(block + 1));
}

size_t StringBuilder::_align(size_t n) const {
    return roundUp(n, 16);
}
