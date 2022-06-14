#ifndef HEADER_GUARD_NKL_CORE_CORE
#define HEADER_GUARD_NKL_CORE_CORE

#include "nk/common/string.hpp"

namespace nkl {

void lang_init();
void lang_deinit();

int lang_runFile(string file);
bool lang_compileFile(string file, string outfile);

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_CORE
