#ifndef HEADER_GUARD_NKL_LANG_LANG
#define HEADER_GUARD_NKL_LANG_LANG

#include "nk/str/string.hpp"

namespace nkl {

using namespace nk;

void lang_init();
void lang_deinit();

int lang_runFile(string path);

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_LANG
