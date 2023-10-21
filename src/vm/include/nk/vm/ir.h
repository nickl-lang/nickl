#ifndef HEADER_GUARD_NK_VM_IR
#define HEADER_GUARD_NK_VM_IR

#include <stddef.h>
#include <stdint.h>

#include "nk/vm/common.h"
#include "nk/vm/value.h"
#include "ntk/allocator.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

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
    NkIrRef_Reg,

    NkIrRef_Global,
    NkIrRef_Const,
    NkIrRef_ExtSym,
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

typedef enum {
    NkIrArg_None,

    NkIrArg_Ref,
    NkIrArg_BlockId,
    NkIrArg_NumValType,
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

DEFINE_ID_TYPE(NkIrBlockId);
DEFINE_ID_TYPE(NkIrConstId);
DEFINE_ID_TYPE(NkIrExtSymId);
DEFINE_ID_TYPE(NkIrGlobalVarId);
DEFINE_ID_TYPE(NkIrLocalVarId);
DEFINE_ID_TYPE(NkIrShObjId);

typedef struct NkIrProg_T *NkIrProg;
typedef struct NkIrFunct_T *NkIrFunct;

NkIrProg nkir_createProgram();
void nkir_deinitProgram(NkIrProg p);

typedef struct NkIrNativeClosure_T *NkIrNativeClosure;

NkIrFunct nkir_makeFunct(NkIrProg p);
NkIrBlockId nkir_makeBlock(NkIrProg p);
NkIrShObjId nkir_makeShObj(NkIrProg p, nks name);
NkIrNativeClosure nkir_makeNativeClosure(NkIrProg p, NkIrFunct funct);

void nkir_startFunct(NkIrFunct funct, nks name, nktype_t fn_t);

void nkir_startIncompleteFunct(NkIrFunct funct, nks name, NktFnInfo const *fn_info);
void nkir_finalizeIncompleteFunct(NkIrFunct funct, nktype_t fn_t);

void nkir_discardFunct(NkIrFunct funct);

nktype_t nkir_functGetType(NkIrFunct funct);
NktFnInfo *nkir_incompleteFunctGetInfo(NkIrFunct funct);

nkval_t nkir_constGetValue(NkIrProg p, NkIrConstId cnst);
nkval_t nkir_refDeref(NkIrProg p, NkIrRef ref);

void nkir_startBlock(NkIrProg p, NkIrBlockId block_id, nks name);

void nkir_activateFunct(NkIrProg p, NkIrFunct funct);
void nkir_activateBlock(NkIrProg p, NkIrBlockId block_id);

NkIrLocalVarId nkir_makeLocalVar(NkIrProg p, nktype_t type);
NkIrGlobalVarId nkir_makeGlobalVar(NkIrProg p, nktype_t type);
NkIrConstId nkir_makeConst(NkIrProg p, nkval_t val);

NkIrExtSymId nkir_makeExtSym(NkIrProg p, NkIrShObjId so, nks name, nktype_t type);

NkIrRef nkir_makeFrameRef(NkIrProg p, NkIrLocalVarId var);
NkIrRef nkir_makeArgRef(NkIrProg p, size_t index);
NkIrRef nkir_makeRetRef(NkIrProg p);
NkIrRef nkir_makeGlobalRef(NkIrProg p, NkIrGlobalVarId var);
NkIrRef nkir_makeConstRef(NkIrProg p, NkIrConstId cnst);
NkIrRef nkir_makeRegRef(NkIrProg p, NkIrRegister reg, nktype_t type);
NkIrRef nkir_makeExtSymRef(NkIrProg p, NkIrExtSymId sym);

NkIrInstr nkir_make_nop();
NkIrInstr nkir_make_enter();
NkIrInstr nkir_make_leave();
NkIrInstr nkir_make_ret();

NkIrInstr nkir_make_jmp(NkIrBlockId label);
NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrBlockId label);
NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrBlockId label);

NkIrInstr nkir_make_cast(NkIrRef dst, nktype_t type, NkIrRef arg);

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef fn, NkIrRef args);

#define U(N) NkIrInstr nkir_make_##N(NkIrRef dst, NkIrRef arg);
#define B(N) NkIrInstr nkir_make_##N(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nk/vm/ir.inl"

void nkir_gen(NkIrProg p, NkIrInstr instr);

void nkir_inspect(NkIrProg p, NkStringBuilder *sb);
void nkir_inspectRef(NkIrProg p, NkIrRef ref, NkStringBuilder *sb);
void nkir_inspectFunct(NkIrFunct fn, NkStringBuilder *sb);
void nkir_inspectExtSyms(NkIrProg p, NkStringBuilder *sb);

void nkir_invoke(nkval_t fn, nkval_t ret, nkval_t args);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_IR
