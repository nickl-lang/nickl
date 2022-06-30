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

namespace nk {

void string_escape(StringBuilder &sb, string str) {
    for (size_t i = 0; i < str.size; i++) {
        switch (str[i]) {
        case '\a':
            sb << "\\a";
            break;
        case '\b':
            sb << "\\b";
            break;
        case '\f':
            sb << "\\f";
            break;
        case '\n':
            sb << "\\n";
            break;
        case '\r':
            sb << "\\r";
            break;
        case '\t':
            sb << "\\t";
            break;
        case '\v':
            sb << "\\v";
            break;
        case '\0':
            sb << "\\0";
            break;
        case '\'':
            sb << "\\'";
            break;
        case '\\':
            sb << "\\\\";
            break;
        default:
            sb << str[i];
            break;
        }
    }
}

void string_unescape(StringBuilder &sb, string str) {
    for (size_t i = 0; i < str.size; i++) {
        if (str[i] == '\\' && i < str.size - 1) {
            switch (str[++i]) {
            case 'a':
                sb << '\a';
                break;
            case 'b':
                sb << '\b';
                break;
            case 'f':
                sb << '\f';
                break;
            case 'n':
                sb << '\n';
                break;
            case 'r':
                sb << '\r';
                break;
            case 't':
                sb << '\t';
                break;
            case 'v':
                sb << '\v';
                break;
            case '0':
                sb << '\0';
                break;
            default:
                sb << str[i];
                break;
            }
        } else {
            sb << str[i];
        }
    }
}

} // namespace nk
