#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR
#define HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {

void test_ir_plus(ir::ProgramBuilder &b);
void test_ir_not(ir::ProgramBuilder &b);
void test_ir_atan(ir::ProgramBuilder &b);
void test_ir_pi(ir::ProgramBuilder &b);
void test_ir_rsqrt(ir::ProgramBuilder &b);
void test_ir_vec2LenSquared(ir::ProgramBuilder &b);
void test_ir_modf(ir::ProgramBuilder &b);
void test_ir_intPart(ir::ProgramBuilder &b);
void test_ir_call10Times(ir::ProgramBuilder &b, type_t fn);
void test_ir_hasZeroByte32(ir::ProgramBuilder &b);
void test_ir_readToggleGlobal(ir::ProgramBuilder &b);
void test_ir_callNativeFunc(ir::ProgramBuilder &b, void *fn_ptr);
void test_ir_callNativeAdd(ir::ProgramBuilder &b, void *fn_ptr);
void test_ir_callExternalPrintf(ir::ProgramBuilder &b, string libname);
void test_ir_getSetExternalVar(ir::ProgramBuilder &b, string libname);
void test_ir_main_argc(ir::ProgramBuilder &b);
void test_ir_main_pi(ir::ProgramBuilder &b, string libc_name);
void test_ir_main_vec2LenSquared(ir::ProgramBuilder &b, string libc_name);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_TEST_IR
