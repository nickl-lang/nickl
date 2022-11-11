#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nk/str/string_builder.hpp"

namespace nkl {

using namespace nk;

struct Compiler {
    StringBuilder &err;

    bool runFile(string filename);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_COMPILER
