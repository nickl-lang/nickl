#ifndef HEADER_GUARD_NK_COMMON_FILE_UTILS
#define HEADER_GUARD_NK_COMMON_FILE_UTILS

#include "nk/mem/allocator.hpp"
#include "nk/str/string.hpp"

namespace nk {

Slice<char> file_read(string path, Allocator &allocator);

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_FILE_UTILS
