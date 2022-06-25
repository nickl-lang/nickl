#include "nk/common/string_builder.hpp"

#include <cassert>
#include <cstdarg>
#include <limits>

#include "nk/common/utils.hpp"

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

void StringBuilder::reserve(size_t n) {
    m_arena.reserve(n);
}

int StringBuilder::print(string str) {
    str.copy(m_arena.push(str.size));
    return str.size;
}

int StringBuilder::print(char const *str) {
    return print(cs2s(str));
}

int StringBuilder::print(char c) {
    return print({&c, 1});
}

int StringBuilder::print(int8_t val) {
    return printf("%hhi", val);
}

int StringBuilder::print(int16_t val) {
    return printf("%hi", val);
}

int StringBuilder::print(int32_t val) {
    return printf("%i", val);
}

int StringBuilder::print(int64_t val) {
    return printf("%lli", val);
}

int StringBuilder::print(uint8_t val) {
    return printf("%hhu", val);
}

int StringBuilder::print(uint16_t val) {
    return printf("%hu", val);
}

int StringBuilder::print(uint32_t val) {
    return printf("%u", val);
}

int StringBuilder::print(uint64_t val) {
    return printf("%llu", val);
}

int StringBuilder::print(float val) {
    return printf("%.*g", std::numeric_limits<float>::max_digits10, val);
}

int StringBuilder::print(double val) {
    return printf("%.*lg", std::numeric_limits<double>::max_digits10, val);
}

int StringBuilder::print(void *ptr) {
    return printf("%p", ptr);
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

string StringBuilder::moveStr(Allocator &allocator) {
    return moveStr({allocator.alloc<char>(size() + 1), size() + 1});
}

string StringBuilder::moveStr(Slice<char> dst) {
    *this << '\0';
    size_t const byte_count = size();
    assert(dst.size >= byte_count && "dst buffer size is too small");
    m_arena.copy(dst.data);
    m_arena.deinit();
    return {dst.data, byte_count - 1};
}
