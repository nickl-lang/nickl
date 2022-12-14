#ifndef HEADER_GUARD_NK_VM_IR
#define HEADER_GUARD_NK_VM_IR

#include <stddef.h>
#include <stdint.h>

#include "nk/vm/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REG_SIZE 8

typedef enum {
    NkIrReg_A,
    NkIrReg_B,
    NkIrReg_C,
    NkIrReg_D,
    NkIrReg_E,
    NkIrReg_F,

    NkIrReg_Count,
} NkIrRegister;

typedef enum {
#define X(NAME) nkir_##NAME,
#include "nk/vm/ir.inl"

    nkir_count,
} NkIrCode;

extern char const *s_ir_names[];

typedef enum {
    NkIrRef_None,
    NkIrRef_Frame,
    NkIrRef_Arg,
    NkIrRef_Ret,
    NkIrRef_Global,
    NkIrRef_Const,
    NkIrRef_Reg,

    NkIrRef_ExtVar,
} NkIrRefType;

typedef struct {
    union {
        void *data;
        size_t index;
    };
    size_t offset;
    size_t post_offset;
    nk_type_t type;
    NkIrRefType ref_type;
    bool is_indirect;
} NkIrRef;

typedef NkIrRef const *NkIrRefPtr;

void nkir_ref_plus(NkIrRef *ref, size_t offset);
void nkir_ref_plusAs(NkIrRef *ref, size_t offset, nk_type_t type);
void nkir_ref_deref(NkIrRef *ref);
void nkir_ref_derefAs(NkIrRef *ref, nk_type_t type);
void nkir_ref_as(NkIrRef *ref, nk_type_t type);

typedef enum {
    NkIrArg_None,

    NkIrArg_Ref,
    NkIrArg_BlockId,
    NkIrArg_FunctId,
    NkIrArg_ExtFunctId,
} NkIrArgType;

typedef struct {
    union {
        NkIrRef ref;
        size_t id;
    };
    NkIrArgType arg_type;
} NkIrArg;

typedef struct {
    NkIrArg arg[3];
    uint16_t code;
} NkIrInstr;

typedef NkIrInstr const *NkIrInstrPtr;

DEFINE_ID_TYPE(NkIrFunctId);
DEFINE_ID_TYPE(NkIrBlockId);
DEFINE_ID_TYPE(NkIrShObjId);
DEFINE_ID_TYPE(NkIrLocalVarId);
DEFINE_ID_TYPE(NkIrGlobalVarId);
DEFINE_ID_TYPE(NkIrExtVarId);
DEFINE_ID_TYPE(NkIrExtFunctId);

typedef struct NkIrProgram_T *NkIrProg;

NkIrProg nkir_createProgram();
void nkir_deinitProgram(NkIrProg p);

NkIrFunctId nkir_makeFunct(NkIrProg p);
NkIrBlockId nkir_makeBlock(NkIrProg p);
NkIrShObjId nkir_makeShObj(NkIrProg p, char const *name);

void nkir_startFunct(NkIrFunctId funct_id, char const *name, nk_type_t fn_t);
void nkir_startBlock(NkIrBlockId block_id, char const *name);

void nkir_activateFunct(NkIrFunctId funct_id);
void nkir_activateBlock(NkIrBlockId block_id);

NkIrLocalVarId nkir_makeLocalVar(NkIrProg p, nk_type_t type);
NkIrGlobalVarId nkir_makeGlobalVar(NkIrProg p, nk_type_t type);

NkIrExtVarId nkir_makeExtVar(NkIrProg p, NkIrShObjId so, char const *sym, nk_type_t type);
NkIrExtFunctId nkir_makeExtFunct(NkIrProg p, NkIrShObjId so, char const *symbol, nk_type_t fn_t);

NkIrRef nkir_makeFrameRef(NkIrProg p, NkIrLocalVarId var);
NkIrRef nkir_makeArgRef(NkIrProg p, size_t index);
NkIrRef nkir_makeRetRef(NkIrProg p);
NkIrRef nkir_makeGlobalRef(NkIrProg p, NkIrGlobalVarId var);
NkIrRef nkir_makeConstRef(NkIrProg p, nk_value_t val);
NkIrRef nkir_makeRegRef(NkIrProg p, NkIrRegister reg, nk_type_t type);
NkIrRef nkir_makeExtVarRef(NkIrProg p, NkIrExtVarId var);

NkIrInstr nkir_make_nop(NkIrProg p);
NkIrInstr nkir_make_enter(NkIrProg p);
NkIrInstr nkir_make_leave(NkIrProg p);
NkIrInstr nkir_make_ret(NkIrProg p);

NkIrInstr nkir_make_jmp(NkIrProg p, NkIrBlockId label);
NkIrInstr nkir_make_jmpz(NkIrProg p, NkIrRefPtr cond, NkIrBlockId label);
NkIrInstr nkir_make_jmpnz(NkIrProg p, NkIrRefPtr cond, NkIrBlockId label);

NkIrInstr nkir_make_cast(NkIrProg p, NkIrRefPtr dst, nk_type_t type, NkIrRefPtr arg);

NkIrInstr nkir_make_call(NkIrProg p, NkIrRefPtr dst, NkIrFunctId funct, NkIrRefPtr args);
NkIrInstr nkir_make_call_ext(NkIrProg p, NkIrRefPtr dst, NkIrExtFunctId funct, NkIrRefPtr args);
NkIrInstr nkir_make_call_indir(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr funct, NkIrRefPtr args);

#define U(NAME) NkIrInstr nkir_make_##NAME(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr arg);
#define B(NAME) \
    NkIrInstr nkir_make_##NAME(NkIrProg p, NkIrRefPtr dst, NkIrRefPtr lhs, NkIrRefPtr rhs);
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstrPtr instr);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_IR
