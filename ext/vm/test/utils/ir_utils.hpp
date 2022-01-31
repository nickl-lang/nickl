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

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
