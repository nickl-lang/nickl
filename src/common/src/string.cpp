#include "nk/common/string.hpp"

#include <cstring>

std::string_view std_view(nkstr str) {
    return std::string_view{str.data, str.size};
}

std::string std_str(nkstr str) {
    return std::string{std_view(str)};
}

std::ostream &operator<<(std::ostream &stream, nkstr str) {
    return stream << std::string_view{str.data, str.size};
}

nkstr cs2s(char const *str) {
    return {str, std::strlen(str)};
}
