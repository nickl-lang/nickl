#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include "nk/common/arena.hpp"
#include "nk/common/private/string_builder_base.hpp"

namespace nk {

struct StringBuilder : StringBuilderBase<StringBuilder> {
private:
    using Base = StringBuilderBase<StringBuilder>;

public:
    void reserve(size_t n);

    size_t size() const {
        return m_arena.size;
    }

    int printf(char const *fmt, ...);

    string moveStr(Allocator &allocator);
    string moveStr(Slice<char> dst);

private:
    Arena<char> m_arena;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
