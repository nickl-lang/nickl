#ifndef HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER

#include "nk/str/string_builder.hpp"

namespace nk {

struct StaticStringBuilder : StringBuilder {
public:
    StaticStringBuilder(Slice<char> dst)
        : m_dst{dst}
        , m_size{0} {
    }

    size_t size() const override {
        return m_dst.size;
    }

    int vprintf(char const *fmt, va_list ap) override;

    using StringBuilder::moveStr;
    string moveStr();
    string moveStr(Slice<char> dst) override;

private:
    Slice<char> m_dst;
    size_t m_size;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STATIC_STRING_BUILDER
