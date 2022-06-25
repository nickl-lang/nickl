#ifndef HEADER_GUARD_NK_COMMON_FILE_UTILS
#define HEADER_GUARD_NK_COMMON_FILE_UTILS

#include "nk/common/allocator.hpp"
#include "nk/common/string.hpp"

Slice<char> file_read(string path, Allocator &allocator);

#endif // HEADER_GUARD_NK_COMMON_FILE_UTILS
