#ifndef HEADER_GUARD_NKL_CORE_TOKEN
#define HEADER_GUARD_NKL_CORE_TOKEN

#include <cstddef>
#include <cstdint>

#include "nk/str/string.hpp"

namespace nkl {

using namespace nk;

struct Token {
    string text;

    size_t pos;
    size_t lin;
    size_t col;

    size_t id = 0;
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_TOKEN
