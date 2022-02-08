#ifndef HEADER_GUARD_NK_VM_C_COMPILER
#define HEADER_GUARD_NK_VM_C_COMPILER

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {

void translateToC(string filenname, ir::Program const &ir);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_C_COMPILER
