#ifndef HEADER_GUARD_NK_COMMON_STRING_HPP
#define HEADER_GUARD_NK_COMMON_STRING_HPP

#include <iosfwd>
#include <string>
#include <string_view>

#include "nk/common/slice.hpp"
#include "nk/common/string.h"

using NkString = NkSlice<char const>;

inline NkString nk_mkstring(char const *str) {
    auto s = nk_mkstr(str);
    return {s.data, s.size};
}

inline NkString nk_mkstring(nkstr str) {
    return {str.data, str.size};
}

inline std::string_view std_view(nkstr str) {
    return std::string_view{str.data, str.size};
}

inline std::string_view std_view(NkString str) {
    return std::string_view{str.data(), str.size()};
}

inline std::string std_str(nkstr str) {
    return std::string{std_view(str)};
}

inline std::string std_str(NkString str) {
    return std::string{std_view(str)};
}

inline std::ostream &operator<<(std::ostream &stream, nkstr str) {
    return stream << std::string_view{str.data, str.size};
}

#endif // HEADER_GUARD_NK_COMMON_STRING_HPP
