#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include "nk/common/arena.hpp"
#include "nk/common/string_builder_base.hpp"

struct StringBuilder : StringBuilderBase<StringBuilder> {
    void reserve(size_t n);
    size_t size() const;

    int printf(char const *fmt, ...);

    using StringBuilderBase<StringBuilder>::moveStr;
    string moveStr(Slice<char> dst);

private:
    Arena<char> m_arena;
};

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
