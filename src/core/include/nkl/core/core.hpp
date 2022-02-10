#ifndef HEADER_GUARD_NKL_CORE_CORE
#define HEADER_GUARD_NKL_CORE_CORE

namespace nkl {

void lang_init();
void lang_deinit();

int lang_runFile(char const *filename);

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_CORE
