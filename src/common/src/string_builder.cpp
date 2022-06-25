#include "nk/common/string_builder.hpp"

#include <cassert>
#include <cstdarg>

#include "nk/common/utils.hpp"

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

void StringBuilder::reserve(size_t n) {
    m_arena.reserve(n);
}

int StringBuilder::printf(char const *fmt, ...) {
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

size_t StringBuilder::size() const {
    return m_arena.size;
}

string StringBuilder::moveStr(Slice<char> dst) {
    *this << '\0';
    size_t const byte_count = size();
    assert(dst.size >= byte_count && "dst buffer size is too small");
    m_arena.copy(dst.data);
    m_arena.deinit();
    return {dst.data, byte_count - 1};
}
