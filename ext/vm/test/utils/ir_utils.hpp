#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
#define HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {
namespace ir {

void buildTestIr_plus(ProgramBuilder &b);
void buildTestIr_not(ProgramBuilder &b);
void buildTestIr_atan(ProgramBuilder &b);
void buildTestIr_pi(ProgramBuilder &b);
void buildTestIr_rsqrt(ProgramBuilder &b);
void buildTestIr_vec2LenSquared(ProgramBuilder &b);
void buildTestIr_modf(ProgramBuilder &b);
void buildTestIr_intPart(ProgramBuilder &b);
void buildTestIr_call10Times(ProgramBuilder &b, type_t fn);
void buildTestIr_hasZeroByte32(ProgramBuilder &b);
void buildTestIr_readToggleGlobal(ProgramBuilder &b);
void buildTestIr_callNativeFunc(ProgramBuilder &b, void *fn_ptr);
void buildTestIr_callNativeAdd(ProgramBuilder &b, void *fn_ptr);
void buildTestIr_callExternalPrintf(ProgramBuilder &b, string libname);

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
