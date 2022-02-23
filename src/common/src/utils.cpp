#include "nk/common/utils.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>

hash_t hash_seed() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

hash_t hash_array(uint8_t const *begin, uint8_t const *end) {
    static const hash_t seed = hash_seed();
    hash_t hash = seed;
    static constexpr size_t c_step = sizeof(size_t);
    size_t i = 0;
    while (begin + (i + 1) * c_step <= end) {
        hash_combine(&hash, *(size_t const *)(begin + i++ * c_step));
    }
    i *= c_step;
    while (begin + i < end) {
        hash_combine(&hash, begin[i++]);
    }
    return hash;
}

string tmpstr_format(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    string str = tmpstr_vformat(fmt, ap);
    va_end(ap);
    return str;
}

string tmpstr_vformat(char const *fmt, va_list ap) {
    return string_vformat(*_mctx.tmp_allocator, fmt, ap);
}

string string_format(Allocator &allocator, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    string str = string_vformat(allocator, fmt, ap);
    va_end(ap);
    return str;
}

string string_vformat(Allocator &allocator, char const *fmt, va_list ap) {
    va_list ap_copy;

    va_copy(ap_copy, ap);
    size_t const n = std::vsnprintf(nullptr, 0, fmt, ap_copy);
    va_end(ap_copy);

    char *data = allocator.alloc<char>(n + 1);

    va_copy(ap_copy, ap);
    std::vsnprintf(data, n + 1, fmt, ap_copy);
    va_end(ap_copy);

    return string{data, n};
}

string string_unescape(Allocator &allocator, string str) {
    std::ostringstream ss;

    for (size_t i = 0; i < str.size; i++) {
        if (str[i] == '\\' && i < str.size - 1) {
            switch (str[++i]) {
            case 'a':
                ss << '\a';
                break;
            case 'b':
                ss << '\b';
                break;
            case 'f':
                ss << '\f';
                break;
            case 'n':
                ss << '\n';
                break;
            case 'r':
                ss << '\r';
                break;
            case 't':
                ss << '\t';
                break;
            case 'v':
                ss << '\v';
                break;
            case '0':
                ss << '\0';
                break;
            default:
                ss << str[i];
                break;
            }
        } else {
            ss << str[i];
        }
    }

    ss << '\0';

    auto const &std_str = ss.str();
    string res;
    string{std_str.data(), std_str.size()}.copy(res, allocator);
    return res;
}
