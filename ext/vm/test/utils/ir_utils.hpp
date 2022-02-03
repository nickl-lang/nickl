#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
#define HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {
namespace ir {

FunctId buildTestIr_plus(ProgramBuilder &b);
FunctId buildTestIr_not(ProgramBuilder &b);
FunctId buildTestIr_atan(ProgramBuilder &b);
FunctId buildTestIr_pi(ProgramBuilder &b);
FunctId buildTestIr_rsqrt(ProgramBuilder &b);
FunctId buildTestIr_vec2LenSquared(ProgramBuilder &b);
FunctId buildTestIr_modf(ProgramBuilder &b);
FunctId buildTestIr_intPart(ProgramBuilder &b);
FunctId buildTestIr_call10Times(ProgramBuilder &b, type_t fn);
FunctId buildTestIr_hasZeroByte32(ProgramBuilder &b);
void buildTestIr_readToggleGlobal(ProgramBuilder &b);
FunctId buildTestIr_callNative(ProgramBuilder &b, void *fn_ptr);

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
