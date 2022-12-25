#ifndef HEADER_GUARD_NK_COMMON_STRING_HPP
#define HEADER_GUARD_NK_COMMON_STRING_HPP

#include <iosfwd>
#include <string>
#include <string_view>

#include "nk/common/string.h"

std::string_view std_view(nkstr str);
std::string std_str(nkstr str);

std::ostream &operator<<(std::ostream &stream, nkstr str);

#endif // HEADER_GUARD_NK_COMMON_STRING_HPP
