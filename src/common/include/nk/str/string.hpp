#ifndef HEADER_GUARD_NK_COMMON_STRING
#define HEADER_GUARD_NK_COMMON_STRING

#include <iosfwd>
#include <string>
#include <string_view>

#include "nk/ds/slice.hpp"

namespace nk {

using string = Slice<char const>;

std::string_view std_view(string str);
std::string std_str(string str);

std::ostream &operator<<(std::ostream &stream, string str);

string cs2s(char const *str);

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STRING