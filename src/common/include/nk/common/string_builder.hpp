#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <cstddef>

#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"

class StringBuilder {
public:
    void reserve(size_t n);
    int printf(char const *fmt, ...);
    string moveStr(Allocator &allocator);

private:
    struct BlockHeader;

    uint8_t *_push(size_t n);
    void _allocateBlock(size_t n);
    uint8_t *_blockData(BlockHeader *header) const;
    size_t _align(size_t n) const;
    size_t _spaceLeftInBlock() const;

private:
    size_t m_size;
    BlockHeader *m_first_block;
    BlockHeader *m_last_block;
};

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
