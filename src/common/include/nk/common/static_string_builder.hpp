#ifndef HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER

#include "nk/common/private/string_builder_base.hpp"

namespace nk {

struct StaticStringBuilder : StringBuilderBase<StaticStringBuilder> {
private:
    using Base = StringBuilderBase<StaticStringBuilder>;

public:
    StaticStringBuilder(Slice<char> dst)
        : m_dst{dst}
        , m_size{0} {
    }

    size_t size() const {
        return m_dst.size;
    }

    int printf(char const *fmt, ...);

    string moveStr();

private:
    Slice<char> m_dst;
    size_t m_size;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER
