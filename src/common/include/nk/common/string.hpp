#ifndef HEADER_GUARD_NK_COMMON_STRING_HPP
#define HEADER_GUARD_NK_COMMON_STRING_HPP

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include "nk/common/string.h"

inline std::string_view std_view(nkstr str) {
    return std::string_view{str.data, str.size};
}

inline std::string std_str(nkstr str) {
    return std::string{std_view(str)};
}

inline std::ostream &operator<<(std::ostream &stream, nkstr str) {
    return stream << std::string_view{str.data, str.size};
}

namespace std {

template <>
struct hash<::nkstr> {
    size_t operator()(::nkstr slice) {
        return ::hash_array((uint8_t *)&slice.data[0], (uint8_t *)&slice.data[slice.size]);
    }
};

template <>
struct equal_to<::nkstr> {
    size_t operator()(::nkstr lhs, ::nkstr rhs) {
        return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
    }
};

} // namespace std

#endif // HEADER_GUARD_NK_COMMON_STRING_HPP
