#ifndef HEADER_GUARD_NK_COMMON_STRING
#define HEADER_GUARD_NK_COMMON_STRING

#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

#include "nk/common/slice.hpp"

using string = Slice<char const>;

inline std::string_view std_view(string str) {
    return std::string_view{str.data, str.size};
}

inline std::string std_str(string str) {
    return std::string{std_view(str)};
}

inline std::ostream &operator<<(std::ostream &stream, string str) {
    return stream << std::string_view{str.data, str.size};
}

inline string cs2s(char const *str) {
    return string{str, std::strlen(str)};
}

#endif // HEADER_GUARD_NK_COMMON_STRING
