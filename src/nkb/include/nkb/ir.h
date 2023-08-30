#ifndef HEADER_GUARD_NKB_IR
#define HEADER_GUARD_NKB_IR

#include <stddef.h>
#include <stdint.h>

#include "nk/common/allocator.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"
#include "nkb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    nkir_nop = 0,

    nkir_ret,

    nkir_jmp,   // jmp         %label
    nkir_jmpz,  // jmpz  cond, %label
    nkir_jmpnz, // jmpnz cond, %label

    nkir_ext,   // ext   src -> dst
    nkir_trunc, // trunc src -> dst
    nkir_fp2i,  // fp2i  src -> dst
    nkir_i2fp,  // i2fp  src -> dst

    nkir_call, // call proc, (args, ...)

    nkir_mov, // mov src -> dst
    nkir_lea, // lea src -> dst

    nkir_neg, // neg arg      -> dst
    nkir_add, // add lhs, rhs -> dst
    nkir_sub, // sub lhs, rhs -> dst
    nkir_mul, // mul lhs, rhs -> dst
    nkir_div, // div lhs, rhs -> dst
    nkir_mod, // mod lhs, rhs -> dst

    nkir_and, // and lhs, rhs -> dst
    nkir_or,  // or  lhs, rhs -> dst
    nkir_xor, // xor lhs, rhs -> dst
    nkir_lsh, // lsh lhs, rhs -> dst
    nkir_rsh, // rsh lhs, rhs -> dst

    nkir_cmp_eq, // cmp eq lhs, rhs -> dst
    nkir_cmp_ne, // cmp ne lhs, rhs -> dst
    nkir_cmp_lt, // cmp lt lhs, rhs -> dst
    nkir_cmp_le, // cmp le lhs, rhs -> dst
    nkir_cmp_gt, // cmp gt lhs, rhs -> dst
    nkir_cmp_ge, // cmp ge lhs, rhs -> dst

    // nkir_cmpxchg,

    nkir_label,

    NkIrOpcode_Count,
} NkIrOpcode;

char const *nkirOpcodeName(NkIrOpcode code);

typedef enum {
    NkIrRef_None = 0,
    NkIrRef_Frame,
    NkIrRef_Arg,
    NkIrRef_Ret,

    NkIrRef_Data,
    NkIrRef_Rodata,
    NkIrRef_Proc,
    NkIrRef_ExternData,
    NkIrRef_ExternProc,
} NkIrRefKind;

typedef struct {
    size_t index;
    size_t offset;
    nktype_t type;
    NkIrRefKind kind;
    bool is_indirect;
} NkIrRef;

typedef struct {
    NkIrRef const *data;
    size_t size;
} NkIrRefArray;

typedef enum {
    NkIrArg_None,

    NkIrArg_Ref,
    NkIrArg_Label,
    NkIrArg_RefArray,
} NkIrArgKind;

typedef struct {
    union {
        NkIrRef ref;
        size_t id;
        NkIrRefArray refs;
    };
    NkIrArgKind arg_kind;
} NkIrArg;

typedef struct {
    NkIrArg arg[3];
    uint8_t code;
} NkIrInstr;

DEFINE_ID_TYPE(NkIrProc);
DEFINE_ID_TYPE(NkIrLabel);
DEFINE_ID_TYPE(NkIrLocalVar);
DEFINE_ID_TYPE(NkIrGlobalVar);
DEFINE_ID_TYPE(NkIrConst);
DEFINE_ID_TYPE(NkIrExternData);
DEFINE_ID_TYPE(NkIrExternProc);

typedef enum {
    NkCallConv_Nk,
    NkCallConv_Cdecl,

    NkCallConv_Count,
} NkCallConv;

typedef enum {
    NkProcVariadic = 1 << 0,
} NkProcFlags;

typedef struct {
    nktype_t const *data;
    size_t size;
} NkTypeArray;

typedef struct {
    NkTypeArray args_t;
    NkTypeArray ret_t;
    NkCallConv call_conv;
    uint8_t flags;
} NkIrProcInfo;

typedef struct NkIrProg_T *NkIrProg;

NkIrProg nkir_createProgram(NkAllocator alloc);
void nkir_freeProgram(NkIrProg ir);

// Code Generation

NkIrProc nkir_createProc(NkIrProg ir);
NkIrLabel nkir_createLabel(NkIrProg ir, nkstr name);

void nkir_startProc(NkIrProg ir, NkIrProc proc, nkstr name, NkIrProcInfo proc_info);
void nkir_activateProc(NkIrProg ir, NkIrProc proc);

void *nkir_constGetData(NkIrProg ir, NkIrConst cnst);
void *nkir_constRefDeref(NkIrProg ir, NkIrRef ref);

typedef struct {
    NkIrInstr const *data;
    size_t size;
} NkIrInstrArray;

void nkir_gen(NkIrProg ir, NkIrInstrArray instrs);

// References

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, nktype_t type);
NkIrGlobalVar nkir_makeGlobalVar(NkIrProg ir, nktype_t type);
NkIrConst nkir_makeConst(NkIrProg ir, void *data, nktype_t type);
NkIrExternData nkir_makeExternData(NkIrProg ir, nkstr name, nktype_t type);
NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkstr name, NkIrProcInfo proc_info);

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var);
NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index);
NkIrRef nkir_makeRetRef(NkIrProg ir, size_t index);
NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var);
NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst);
NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc);
NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data);
NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc);

// Instructions

NkIrInstr nkir_make_nop();
NkIrInstr nkir_make_ret();

NkIrInstr nkir_make_jmp(NkIrLabel label);
NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label);
NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label);

NkIrInstr nkir_make_ext(NkIrRef dst, NkIrRef src);
NkIrInstr nkir_make_trunc(NkIrRef dst, NkIrRef src);
NkIrInstr nkir_make_fp2i(NkIrRef dst, NkIrRef src);
NkIrInstr nkir_make_i2fp(NkIrRef dst, NkIrRef src);

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args);

NkIrInstr nkir_make_mov(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_lea(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);

NkIrInstr nkir_make_neg(NkIrRef dst, NkIrRef arg);
NkIrInstr nkir_make_add(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_sub(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_mul(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_div(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_mod(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);

NkIrInstr nkir_make_and(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_or(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_xor(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_lsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_rsh(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);

NkIrInstr nkir_make_cmp_eq(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_cmp_ne(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_cmp_lt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_cmp_le(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_cmp_gt(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
NkIrInstr nkir_make_cmp_ge(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);

NkIrInstr nkir_make_label(NkIrLabel label);

// Output

void nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr filepath);

// Execution

typedef struct NkIrRunCtx_T *NkIrRunCtx;

NkIrRunCtx nkir_createRunCtx(NkIrProg ir);
void nkir_freeRunCtx(NkIrRunCtx ctx);

typedef struct {
    void const *data;
    size_t size;
} NkIrPtrArray;

void nkir_invoke(NkIrProc proc, NkIrPtrArray args, NkIrPtrArray ret);

// Inspection

void nkir_inspectProgram(NkIrProg ir, NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IR
