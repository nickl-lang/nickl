#ifndef HEADER_GUARD_NK_COMMON_STRGIN
#define HEADER_GUARD_NK_COMMON_STRGIN

#include <cstring>
#include <ostream>
#include <string_view>

#include "nk/common/slice.hpp"

using string = Slice<char const>;

inline std::string_view std_view(string str) {
    return std::string_view{str.data, str.size};
}

inline std::ostream &operator<<(std::ostream &stream, string str) {
    return stream << std::string_view{str.data, str.size};
}

inline string cs2s(char const *str) {
    return string{str, std::strlen(str)};
}

inline string stds2s(std::string const &str) {
    return string{str.data(), str.size()};
}

#endif // HEADER_GUARD_NK_COMMON_STRGIN
