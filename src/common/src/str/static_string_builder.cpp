#include "nk/str/static_string_builder.hpp"

#include <cassert>
#include <cstdio>

namespace nk {

int StaticStringBuilder::printf(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto const dst = m_dst.slice(m_size);
    int const printf_res = std::vsnprintf(dst.data, dst.size, fmt, ap);
    va_end(ap);

    if (printf_res >= 0) {
        m_size += printf_res;
    }

    assert(m_size <= m_dst.size && "buffer overflow");

    return printf_res;
}

string StaticStringBuilder::moveStr() {
    string const str{m_dst.data, m_size};
    *this << '\0';
    m_size = 0;
    return str;
}

string StaticStringBuilder::moveStr(Slice<char> dst) {
    string const str{m_dst.data, m_size};
    *this << '\0';
    str.copy(dst);
    m_size = 0;
    return str;
}

} // namespace nk
