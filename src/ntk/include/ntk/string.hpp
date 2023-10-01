#ifndef HEADER_GUARD_NTK_STRING_HPP
#define HEADER_GUARD_NTK_STRING_HPP

#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include "ntk/string.h"

inline std::string_view std_view(nks str) {
    return std::string_view{str.data, str.size};
}

inline std::string std_str(nks str) {
    return std::string{std_view(str)};
}

inline std::ostream &operator<<(std::ostream &stream, nks str) {
    return stream << std::string_view{str.data, str.size};
}

namespace std {

template <>
struct hash<::nks> {
    size_t operator()(::nks slice) {
        return ::hash_array((uint8_t *)&slice.data[0], (uint8_t *)&slice.data[slice.size]);
    }
};

template <>
struct equal_to<::nks> {
    size_t operator()(::nks lhs, ::nks rhs) {
        return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
    }
};

} // namespace std

#endif // HEADER_GUARD_NTK_STRING_HPP
