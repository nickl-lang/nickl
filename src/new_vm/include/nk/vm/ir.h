#ifndef HEADER_GUARD_NK_VM_IR
#define HEADER_GUARD_NK_VM_IR

#include <stddef.h>
#include <stdint.h>

#include "nk/common/string.h"
#include "nk/common/string_builder.h"
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

extern char const *s_nk_ir_names[];

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
    nktype_t type;
    NkIrRefType ref_type;
    bool is_indirect;
} NkIrRef;

typedef NkIrRef const *NkIrRefPtr;

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

typedef struct NkIrProg_T *NkIrProg;

NkIrProg nkir_createProgram();
void nkir_deinitProgram(NkIrProg p);

NkIrFunctId nkir_makeFunct(NkIrProg p);
NkIrBlockId nkir_makeBlock(NkIrProg p);
NkIrShObjId nkir_makeShObj(NkIrProg p, nkstr name);

void nkir_startFunct(NkIrProg p, NkIrFunctId funct_id, nkstr name, nktype_t fn_t);
void nkir_startBlock(NkIrProg p, NkIrBlockId block_id, nkstr name);

void nkir_activateFunct(NkIrProg p, NkIrFunctId funct_id);
void nkir_activateBlock(NkIrProg p, NkIrBlockId block_id);

NkIrLocalVarId nkir_makeLocalVar(NkIrProg p, nktype_t type);
NkIrGlobalVarId nkir_makeGlobalVar(NkIrProg p, nktype_t type);

NkIrExtVarId nkir_makeExtSym(NkIrProg p, NkIrShObjId so, nkstr name, nktype_t type);

NkIrRef nkir_makeFrameRef(NkIrProg p, NkIrLocalVarId var);
NkIrRef nkir_makeArgRef(NkIrProg p, size_t index);
NkIrRef nkir_makeRetRef(NkIrProg p);
NkIrRef nkir_makeGlobalRef(NkIrProg p, NkIrGlobalVarId var);
NkIrRef nkir_makeConstRef(NkIrProg p, nkval_t val);
NkIrRef nkir_makeRegRef(NkIrProg p, NkIrRegister reg, nktype_t type);
NkIrRef nkir_makeExtVarRef(NkIrProg p, NkIrExtVarId var);

NkIrInstr nkir_make_nop();
NkIrInstr nkir_make_enter();
NkIrInstr nkir_make_leave();
NkIrInstr nkir_make_ret();

NkIrInstr nkir_make_jmp(NkIrBlockId label);
NkIrInstr nkir_make_jmpz(NkIrRefPtr cond, NkIrBlockId label);
NkIrInstr nkir_make_jmpnz(NkIrRefPtr cond, NkIrBlockId label);

NkIrInstr nkir_make_cast(NkIrRefPtr dst, nktype_t type, NkIrRefPtr arg);

NkIrInstr nkir_make_call(NkIrRefPtr dst, NkIrFunctId funct, NkIrRefPtr args);
NkIrInstr nkir_make_call_ext(NkIrRefPtr dst, NkIrExtFunctId funct, NkIrRefPtr args);
NkIrInstr nkir_make_call_indir(NkIrRefPtr dst, NkIrRefPtr funct, NkIrRefPtr args);

#define U(N) NkIrInstr nkir_make_##N(NkIrRefPtr dst, NkIrRefPtr arg);
#define B(N) NkIrInstr nkir_make_##N(NkIrRefPtr dst, NkIrRefPtr lhs, NkIrRefPtr rhs);
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstrPtr instr);

void nkir_invoke(NkIrProg p, NkIrFunctId fn, nkval_t ret, nkval_t args);

void nkir_inspect(NkIrProg p, NkStringBuilder sb);
void nkir_inspectRef(NkIrProg p, NkIrRefPtr ref, NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_IR
