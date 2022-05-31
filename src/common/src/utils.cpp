#include "nk/common/utils.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

void string_escape(StringBuilder &sb, string str) {
    for (size_t i = 0; i < str.size; i++) {
        switch (str[i]) {
        case '\a':
            sb.print("\\a");
            break;
        case '\b':
            sb.print("\\b");
            break;
        case '\f':
            sb.print("\\f");
            break;
        case '\n':
            sb.print("\\n");
            break;
        case '\r':
            sb.print("\\r");
            break;
        case '\t':
            sb.print("\\t");
            break;
        case '\v':
            sb.print("\\v");
            break;
        case '\0':
            sb.print("\\0");
            break;
        case '\'':
            sb.print("\\'");
            break;
        case '\\':
            sb.print("\\\\");
            break;
        default:
            sb.print(str[i]);
            break;
        }
    }
}

void string_unescape(StringBuilder &sb, string str) {
    for (size_t i = 0; i < str.size; i++) {
        if (str[i] == '\\' && i < str.size - 1) {
            switch (str[++i]) {
            case 'a':
                sb.print('\a');
                break;
            case 'b':
                sb.print('\b');
                break;
            case 'f':
                sb.print('\f');
                break;
            case 'n':
                sb.print('\n');
                break;
            case 'r':
                sb.print('\r');
                break;
            case 't':
                sb.print('\t');
                break;
            case 'v':
                sb.print('\v');
                break;
            case '0':
                sb.print('\0');
                break;
            default:
                sb.print(str[i]);
                break;
            }
        } else {
            sb.print(str[i]);
        }
    }
}
