#include "nk/common/string.hpp"

#include <cstring>

namespace nk {

std::string_view std_view(string str) {
    return std::string_view{str.data, str.size};
}

std::string std_str(string str) {
    return std::string{std_view(str)};
}

std::ostream &operator<<(std::ostream &stream, string str) {
    return stream << std::string_view{str.data, str.size};
}

string cs2s(char const *str) {
    return string{str, std::strlen(str)};
}

} // namespace nk
