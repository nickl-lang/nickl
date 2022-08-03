#ifndef HEADER_GUARD_NKL_LANG_COMPILER_API
#define HEADER_GUARD_NKL_LANG_COMPILER_API

#include "nk/str/string.hpp"
#include "nkl/lang/value.hpp"

namespace nkl {

struct CompilerContext;

extern "C" CompilerContext *nk_compiler_getInstance();

extern "C" bool nk_compiler_defineVar(CompilerContext *c, string name, type_t type);
extern "C" bool nk_compiler_defineVarInit(CompilerContext *c, string name, value_t value);
extern "C" bool nk_compiler_defineConst(CompilerContext *c, string name, value_t value);

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_COMPILER_API
