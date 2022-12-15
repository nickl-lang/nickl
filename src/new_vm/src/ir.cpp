#include "nk/vm/ir.h"

void nkir_ref_plus(NkIrRef *ref, size_t offset) {
}

void nkir_ref_plusAs(NkIrRef *ref, size_t offset, nk_type_t type) {
}

void nkir_ref_deref(NkIrRef *ref) {
}

void nkir_ref_derefAs(NkIrRef *ref, nk_type_t type) {
}

void nkir_ref_as(NkIrRef *ref, nk_type_t type) {
}

NkIrProg nkir_createProgram() {
}

void nkir_deinitProgram(NkIrProg p) {
}

NkIrFunct nkir_makeFunct(NkIrProg p) {
}

NkIrBlock nkir_makeBlock(NkIrProg p) {
}

NkIrShObj nkir_makeShObj(NkIrProg p, char const *name) {
}

void nkir_startFunct(NkIrFunct funct_id, char const *name, nk_type_t fn_t) {
}

void nkir_startBlock(NkIrBlock block_id, char const *name) {
}

void nkir_activateFunct(NkIrFunct funct_id) {
}

void nkir_activateBlock(NkIrBlock block_id) {
}

NkIrLocalVar nkir_makeLocalVar(NkIrProg p, nk_type_t type) {
}

NkIrGlobalVar nkir_makeGlobalVar(NkIrProg p, nk_type_t type) {
}

NkIrExtVar nkir_makeExtVar(NkIrProg p, NkIrShObj so, char const *sym, nk_type_t type) {
}

NkIrExtFunct nkir_makeExtFunct(NkIrProg p, NkIrShObj so, char const *sym, nk_type_t fn_t) {
}

NkIrRef nkir_makeFrameRef(NkIrProg p, NkIrLocalVar var) {
}

NkIrRef nkir_makeArgRef(NkIrProg p, size_t index) {
}

NkIrRef nkir_makeRetRef(NkIrProg p) {
}

NkIrRef nkir_makeGlobalRef(NkIrProg p, NkIrGlobalVar var) {
}

NkIrRef nkir_makeConstRef(NkIrProg p, nk_value_t val) {
}

NkIrRef nkir_makeRegRef(NkIrProg p, NkIrRegister reg, nk_type_t type) {
}

NkIrRef nkir_makeExtVarRef(NkIrProg p, NkIrExtVar var) {
}

NkIrInstr nkir_make_nop(NkIrProg p) {
}

NkIrInstr nkir_make_enter(NkIrProg p) {
}

NkIrInstr nkir_make_leave(NkIrProg p) {
}

NkIrInstr nkir_make_ret(NkIrProg p) {
}

NkIrInstr nkir_make_jmp(NkIrProg p, NkIrBlock label) {
}

NkIrInstr nkir_make_jmpz(NkIrProg p, NkIrRefPtr cond, NkIrBlock label) {
}

NkIrInstr nkir_make_jmpnz(NkIrProg p, NkIrRefPtr cond, NkIrBlock label) {
}

NkIrInstr nkir_make_cast(NkIrProg p, NkIrRefPtr dst, nk_type_t type, NkIrRefPtr arg) {
}

NkIrInstr nkir_make_call(NkIrProg p, NkIrRefPtr dst, NkIrFunct funct, NkIrRefPtr args) {
}

NkIrInstr nkir_make_call_ext(NkIrProg p, NkIrRefPtr dst, NkIrExtFunct funct, NkIrRefPtr args) {
}

NkIrInstr nkir_make_call_indir(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr funct, NkIrRefPtr args) {
}

#define U(NAME)                                                              \
    NkIrInstr nkir_make_##NAME(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr arg) { \
    }
#define B(NAME)                                                                              \
    NkIrInstr nkir_make_##NAME(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr lhs, NkIrRefPtr rhs) { \
    }
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstrPtr instr) {
}

void nkir_invoke(NkIrProg p, NkIrFunct self, nk_value_t ret, nk_value_t args) {
}
