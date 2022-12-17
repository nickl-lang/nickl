#ifndef HEADER_GUARD_NK_COMMON_STRING_HPP
#define HEADER_GUARD_NK_COMMON_STRING_HPP

#include <iosfwd>
#include <string>
#include <string_view>

#include "nk/common/string.h"

std::string_view std_view(string str);
std::string std_str(string str);

std::ostream &operator<<(std::ostream &stream, string str);

#endif // HEADER_GUARD_NK_COMMON_STRING_HPP
