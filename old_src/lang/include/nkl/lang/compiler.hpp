#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nk/ds/hashmap.hpp"
#include "nk/str/string_builder.hpp"
#include "nk/vm/ir.hpp"
#include "nkl/lang/ast.hpp"
#include "nkl/lang/value.hpp"

namespace nkl {

using nk::vm::ir::Program;

struct Compiler {
    StringBuilder &err;
    Program prog{};

    HashMap<Id, type_t> intrinsics{};

    bool compile(NodeRef root);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_COMPILER
