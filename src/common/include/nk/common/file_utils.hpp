#ifndef HEADER_GUARD_NK_COMMON_FILE_UTILS
#define HEADER_GUARD_NK_COMMON_FILE_UTILS

#include "nk/common/array.hpp"
#include "nk/common/string.hpp"

Array<char> file_read(string path);
bool file_exists(string path);

string path_concat(string lhs, string rhs);
bool path_isAbsolute(string path);
string path_current();
string path_parent(string path);

#endif // HEADER_GUARD_NK_COMMON_FILE_UTILS
