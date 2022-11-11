#ifndef HEADER_GUARD_NKL_LANG_COMPILER_API
#define HEADER_GUARD_NKL_LANG_COMPILER_API

#include "nk/str/string.hpp"
#include "nkl/lang/value.hpp"

namespace nkl {

using namespace nk;

extern "C" bool nk_compiler_defineVar(string name, type_t type);
extern "C" bool nk_compiler_defineVarInit(string name, value_t value);
extern "C" bool nk_compiler_defineConst(string name, value_t value);

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_COMPILER_API
