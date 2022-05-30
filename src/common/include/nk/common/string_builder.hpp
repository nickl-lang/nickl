#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include "nk/common/block_allocator.h"
#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"

class StringBuilder {
public:
    void reserve(size_t n);
    int printf(char const *fmt, ...);
    string moveStr(Allocator &allocator);

private:
    BlockAllocator m_allocator;
};

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
