#include "nk/str/dynamic_string_builder.hpp"

#include <cstdarg>
#include <cstdio>

#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nk {

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

void DynamicStringBuilder::deinit() {
    EASY_BLOCK("DynamicStringBuilder::deinit", profiler::colors::Grey200)
    m_arena.deinit();
}

void DynamicStringBuilder::reserve(size_t n) {
    EASY_BLOCK("DynamicStringBuilder::reserve", profiler::colors::Grey200)
    m_arena.reserve(n);
}

int DynamicStringBuilder::vprintf(char const *fmt, va_list ap) {
    EASY_BLOCK("DynamicStringBuilder::printf", profiler::colors::Grey200)

    char buf[PRINTF_BUFFER_SIZE];

    va_list ap_copy;

    va_copy(ap_copy, ap);
    int const printf_res = std::vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, ap_copy);
    va_end(ap_copy);

    int const byte_count = printf_res + 1;

    if (byte_count > (int)PRINTF_BUFFER_SIZE) {
        va_copy(ap_copy, ap);
        std::vsnprintf(m_arena.push(byte_count), byte_count, fmt, ap_copy);
        va_end(ap_copy);

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
    m_arena.clear();
    return {dst.data, len};
}

} // namespace nk
