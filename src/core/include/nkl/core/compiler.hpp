#ifndef HEADER_GUARD_NKL_CORE_COMPILER
#define HEADER_GUARD_NKL_CORE_COMPILER

#include "nk/vm/ir.hpp"
#include "nkl/core/ast.hpp"

namespace nkl {

using nk::vm::ir::Program;

struct Compiler {
    Program prog;
    string err;

    bool compile(node_ref_t root);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_COMPILER
