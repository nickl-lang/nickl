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

    if (printf_res > 0) {
        size_t const old_size = m_size;
        m_size = minu(m_size + printf_res, m_dst.size - 1);
        return m_size - old_size;
    }

    return printf_res;
}

string StaticStringBuilder::moveStr() {
    string const str{m_dst.data, m_size};
    *this << '\0';
    m_size = 0;
    return str;
}

string StaticStringBuilder::moveStr(Slice<char> dst) {
    return moveStr().copy(dst);
}

} // namespace nk
