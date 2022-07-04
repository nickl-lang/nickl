#include "nk/common/dynamic_string_builder.hpp"

#include <cstdarg>
#include <cstdio>

#include "nk/common/utils.hpp"

namespace nk {

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

void DynamicStringBuilder::reserve(size_t n) {
    m_arena.reserve(n);
}

int DynamicStringBuilder::printf(char const *fmt, ...) {
    char buf[PRINTF_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    int const printf_res = std::vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, ap);
    int const byte_count = printf_res + 1;
    va_end(ap);

    if (byte_count > (int)PRINTF_BUFFER_SIZE) {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(m_arena.push(byte_count), byte_count, fmt, ap);
        va_end(ap);

        m_arena.pop();
    } else if (byte_count > 0) {
        Slice<char const>{buf, (size_t)(byte_count - 1)}.copy(m_arena.push(byte_count - 1));
    }

    return printf_res;
}

string DynamicStringBuilder::moveStr(Slice<char> dst) {
    size_t const len = size();
    *this << '\0';
    m_arena.copy(dst);
    m_arena.deinit();
    return {dst.data, len};
}

} // namespace nk
