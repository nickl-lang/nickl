#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR
#define HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {
namespace ir {

void test_ir_plus(ProgramBuilder &b);
void test_ir_not(ProgramBuilder &b);
void test_ir_atan(ProgramBuilder &b);
void test_ir_pi(ProgramBuilder &b);
void test_ir_rsqrt(ProgramBuilder &b);
void test_ir_vec2LenSquared(ProgramBuilder &b);
void test_ir_modf(ProgramBuilder &b);
void test_ir_intPart(ProgramBuilder &b);
void test_ir_call10Times(ProgramBuilder &b, type_t fn);
void test_ir_hasZeroByte32(ProgramBuilder &b);
void test_ir_readToggleGlobal(ProgramBuilder &b);
void test_ir_callNativeFunc(ProgramBuilder &b, void *fn_ptr);
void test_ir_callNativeAdd(ProgramBuilder &b, void *fn_ptr);
void test_ir_callExternalPrintf(ProgramBuilder &b, string libname);
void test_ir_getSetExternalVar(ProgramBuilder &b, string libname);
void test_ir_main(ProgramBuilder &b);

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR
