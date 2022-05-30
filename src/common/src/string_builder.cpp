#include "nk/common/string_builder.hpp"

#include <cstdarg>

#include "nk/common/utils.hpp"

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

struct StringBuilder::BlockHeader {
    BlockHeader *next;
    size_t size;
    size_t capacity;
};

void StringBuilder::reserve(size_t n) {
    if (n > _spaceLeftInBlock()) {
        _allocateBlock(n);
    }
}

int StringBuilder::printf(char const *fmt, ...) {
    char ar[PRINTF_BUFFER_SIZE];
    char *buf = ar;
    defer {
        if (buf != ar) {
            ::free(buf);
        }
    };

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, ap);
    va_end(ap);

    if (n >= (int)PRINTF_BUFFER_SIZE) {
        buf = (char *)::malloc(n + 1);

        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, n + 1, fmt, ap);
        va_end(ap);
    }

    if (n >= 0) {
        size_t first_part_size = minu(_spaceLeftInBlock(), (size_t)n + 1);

        size_t size = first_part_size;
        ::memcpy(_push(size), buf, size);

        size = n + 1 - first_part_size;
        ::memcpy(_push(size), buf + first_part_size, size);
    }

    return n;
}

string StringBuilder::moveStr(Allocator &allocator) {
    if (m_size) {
        char *mem = (char *)allocator.alloc(m_size);
        size_t offset = 0;
        for (BlockHeader *block = m_first_block; block;) {
            BlockHeader *cur_block = block;
            block = block->next;
            ::memcpy(mem + offset, _blockData(cur_block), cur_block->size);
            offset += cur_block->size;
            ::free(cur_block);
        }
        return {mem, m_size - 1};
    } else {
        return {};
    }
}

uint8_t *StringBuilder::_push(size_t n) {
    if (n) {
        reserve(n);
        m_last_block->size += n;
        m_size += n;
        return _blockData(m_last_block) + m_last_block->size - n;
    } else {
        return nullptr;
    }
}

void StringBuilder::_allocateBlock(size_t n) {
    auto const header_size = _align(sizeof(BlockHeader));
    n = ceilToPowerOf2(n + header_size);
    n = maxu(n, (m_last_block ? m_last_block->capacity << 1 : 0));

    BlockHeader *block = (BlockHeader *)::malloc(n);

    block->next = nullptr;
    block->size = 0;
    block->capacity = n - header_size;

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

size_t StringBuilder::_spaceLeftInBlock() const {
    return m_last_block ? m_last_block->capacity - m_last_block->size : 0;
}
