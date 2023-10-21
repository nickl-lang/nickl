#ifndef HEADER_GUARD_NKB_IR
#define HEADER_GUARD_NKB_IR

#include <stddef.h>
#include <stdint.h>

#include "nkb/common.h"
#include "ntk/allocator.h"
#include "ntk/id.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

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
    NkIrRef_Rodata,
    NkIrRef_Data,

    NkIrRef_Proc,
    NkIrRef_ExternData,
    NkIrRef_ExternProc,
    NkIrRef_Address,
} NkIrRefKind;

typedef struct {
    size_t index;
    size_t offset;
    size_t post_offset;
    nktype_t type;
    NkIrRefKind kind;
    uint8_t indir;
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
    NkIrArg_Comment,
} NkIrArgKind;

typedef enum {
    NkIrVisibility_Default,
    NkIrVisibility_Hidden,
    NkIrVisibility_Protected,
    NkIrVisibility_Internal,
    NkIrVisibility_Local,
} NkIrVisibility;

typedef struct {
    union {
        NkIrRef ref;
        NkIrRefArray refs;
        size_t id;
        nks comment;
    };
    NkIrArgKind kind;
} NkIrArg;

typedef struct {
    NkIrArg arg[3];
    size_t line;
    uint8_t code;
} NkIrInstr;

#define DEFINE_IDX_TYPE(NAME) \
    typedef struct {          \
        size_t idx;           \
    } NAME

DEFINE_IDX_TYPE(NkIrProc);
DEFINE_IDX_TYPE(NkIrLabel);
DEFINE_IDX_TYPE(NkIrLocalVar);
DEFINE_IDX_TYPE(NkIrGlobalVar);
DEFINE_IDX_TYPE(NkIrConst);
DEFINE_IDX_TYPE(NkIrExternData);
DEFINE_IDX_TYPE(NkIrExternProc);

#undef DEFINE_IDX_TYPE

#define NKIR_INVALID_IDX ((size_t)-1ul)

typedef struct NkIrProg_T *NkIrProg;

NkIrProg nkir_createProgram(NkArena *arena);

nks nkir_getErrorString(NkIrProg ir);

// Code Generation

NkIrProc nkir_createProc(NkIrProg ir);
NkIrLabel nkir_createLabel(NkIrProg ir, nkid name);

nkav_typedef(nkid, nkid_array);
void nkir_startProc(
    NkIrProg ir,
    NkIrProc proc,
    nkid name,
    nktype_t proc_t,
    nkid_array arg_names,
    nkid file,
    size_t line,
    NkIrVisibility vis);
void nkir_activateProc(NkIrProg ir, NkIrProc proc);

void nkir_finishProc(NkIrProg ir, NkIrProc proc, size_t line);

void *nkir_constGetData(NkIrProg ir, NkIrConst cnst);
void *nkir_constRefDeref(NkIrProg ir, NkIrRef ref);

nkav_typedef(NkIrInstr const, NkIrInstrArray);
void nkir_gen(NkIrProg ir, NkIrInstrArray instrs);

void nkir_setLine(NkIrProg ir, size_t line);

void nkir_enter(NkIrProg ir);
void nkir_leave(NkIrProg ir);

// References

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, nkid name, nktype_t type);
NkIrGlobalVar nkir_makeGlobalVar(NkIrProg ir, nkid name, nktype_t type, NkIrVisibility vis);
NkIrConst nkir_makeConst(NkIrProg ir, nkid name, nktype_t type, NkIrVisibility vis);
NkIrExternData nkir_makeExternData(NkIrProg ir, nkid lib, nkid name, nktype_t type);
NkIrExternProc nkir_makeExternProc(NkIrProg ir, nkid lib, nkid name, nktype_t proc_t);

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var);
NkIrRef nkir_makeArgRef(NkIrProg ir, size_t index);
NkIrRef nkir_makeRetRef(NkIrProg ir);
NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrGlobalVar var);
NkIrRef nkir_makeRodataRef(NkIrProg ir, NkIrConst cnst);
NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc);
NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data);
NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc);

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref, nktype_t ptr_t);

// Instructions

NkIrInstr nkir_make_nop(NkIrProg ir);
NkIrInstr nkir_make_ret(NkIrProg ir);

NkIrInstr nkir_make_jmp(NkIrProg ir, NkIrLabel label);
NkIrInstr nkir_make_jmpz(NkIrProg ir, NkIrRef cond, NkIrLabel label);
NkIrInstr nkir_make_jmpnz(NkIrProg ir, NkIrRef cond, NkIrLabel label);

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args);

#define UNA_IR(NAME) NkIrInstr CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef arg);
#define BIN_IR(NAME) NkIrInstr CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#define DBL_IR(NAME1, NAME2) \
    NkIrInstr CAT(nkir_make_, CAT(NAME1, CAT(_, NAME2)))(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nkb/ir.inl"

NkIrInstr nkir_make_syscall(NkIrProg ir, NkIrRef dst, NkIrRef n, NkIrRefArray args);

NkIrInstr nkir_make_label(NkIrLabel label);

NkIrInstr nkir_make_comment(NkIrProg ir, nks comment);

// Output

typedef struct {
    nks compiler_binary;
    nkav_type(nks const) additional_flags;
    nks output_filename;
    NkbOutputKind output_kind;
    bool quiet;
} NkIrCompilerConfig;

bool nkir_write(NkIrProg ir, NkArena *arena, NkIrCompilerConfig conf);

// Execution

typedef struct NkIrRunCtx_T *NkIrRunCtx;

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena);
void nkir_freeRunCtx(NkIrRunCtx ctx);

bool nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, void **args, void **ret);

nks nkir_getRunErrorString(NkIrRunCtx ctx);

// Inspection

#ifdef ENABLE_LOGGING
void nkir_inspectProgram(NkIrProg ir, nk_stream out);
void nkir_inspectData(NkIrProg ir, nk_stream out);
void nkir_inspectExternSyms(NkIrProg ir, nk_stream out);
void nkir_inspectProc(NkIrProg ir, NkIrProc proc, nk_stream out);
void nkir_inspectRef(NkIrProg ir, NkIrProc proc, NkIrRef ref, nk_stream out);
#endif // ENABLE_LOGGING

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IR
