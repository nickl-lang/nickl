#ifndef HEADER_GUARD_NK_COMMON_COMMON
#define HEADER_GUARD_NK_COMMON_COMMON

#include <cstddef>
#include <cstring>
#include <string_view>

using hash_t = size_t;

struct string {
    size_t size;
    char const *data;

    std::string_view view() const {
        return std::string_view{data, size};
    }
};

inline string cstr_to_str(char const *str) {
    return string{std::strlen(str), str};
}

#endif // HEADER_GUARD_NK_COMMON_COMMON
