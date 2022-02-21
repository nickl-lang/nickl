#ifndef HEADER_GUARD_NKL_CORE_COMPILER
#define HEADER_GUARD_NKL_CORE_COMPILER

#include "nk/common/hashmap.hpp"
#include "nk/vm/ir.hpp"
#include "nkl/core/ast.hpp"
#include "nkl/core/value.hpp"

namespace nkl {

using nk::vm::ir::Program;

struct Compiler {
    Program prog;
    string err;

    HashMap<Id, type_t> intrinsics;

    void init();
    void deinit();

    bool compile(node_ref_t root);
};

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_COMPILER
