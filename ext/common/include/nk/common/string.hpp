#ifndef HEADER_GUARD_NK_COMMON_STRGIN
#define HEADER_GUARD_NK_COMMON_STRGIN

#include <cstring>
#include <ostream>
#include <string_view>

#include "nk/common/slice.hpp"

struct string : Slice<char const> {
    std::string_view view() const {
        return std::string_view{data, size};
    }
};

inline std::ostream &operator<<(std::ostream &stream, string str) {
    return stream << std::string_view{str.data, str.size};
}

inline string cstr_to_str(char const *str) {
    return string{str, std::strlen(str)};
}

#endif // HEADER_GUARD_NK_COMMON_STRGIN
