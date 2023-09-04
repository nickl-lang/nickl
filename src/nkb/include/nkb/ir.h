#ifndef HEADER_GUARD_NKB_IR
#define HEADER_GUARD_NKB_IR

#include <stddef.h>
#include <stdint.h>

#include "nk/common/allocator.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nkb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define IR(NAME) CAT(nkir_, NAME),
#include "nkb/ir.inl"

    NkIrOpcode_Count,
} NkIrOpcode;

char const *nkirOpcodeName(uint8_t code);

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
    NkIrRef_Reloc,
} NkIrRefKind;

typedef struct {
    size_t index;
    size_t offset;
    size_t post_offset;
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
    NkIrArg_RefArray,
    NkIrArg_Label,
} NkIrArgKind;

typedef struct {
    union {
        NkIrRef ref;
        NkIrRefArray refs;
        size_t id;
    };
    NkIrArgKind kind;
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

NkIrProg nkir_createProgram(NkAllocator alloc, nktype_t size_type);
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

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref);

// Instructions

NkIrInstr nkir_make_nop();
NkIrInstr nkir_make_ret();

NkIrInstr nkir_make_jmp(NkIrLabel label);
NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label);
NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label);

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args);

NkIrInstr nkir_make_label(NkIrLabel label);

#define UNA_IR(NAME) NkIrInstr CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg);
#define BIN_IR(NAME) NkIrInstr CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#define DBL_IR(NAME1, NAME2) \
    NkIrInstr CAT(nkir_make_, CAT(NAME1, CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nkb/ir.inl"

// Output

bool nkir_write(NkIrProg ir, NkbOutputKind kind, nkstr out_file);

// Execution

typedef struct NkIrRunCtx_T *NkIrRunCtx;

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena);
void nkir_freeRunCtx(NkIrRunCtx ctx);

void nkir_defineExternSym(NkIrRunCtx ctx, nkstr name, void *data);

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, void *args, void *ret);

// Inspection

#ifdef ENABLE_LOGGING
void nkir_inspectProgram(NkIrProg ir, NkStringBuilder sb);
void nkir_inspectData(NkIrProg ir, NkStringBuilder sb);
void nkir_inspectExternSyms(NkIrProg ir, NkStringBuilder sb);
void nkir_inspectProc(NkIrProg ir, NkIrProc proc, NkStringBuilder sb);
void nkir_inspectRef(NkIrProg ir, NkIrRef ref, NkStringBuilder sb);
#endif // ENABLE_LOGGING

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IR
