#ifndef NKB_IR_H_
#define NKB_IR_H_

#include "nkb/common.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define IR(NAME) NK_CAT(nkir_, NAME),
#include "nkb/ir.inl"

    NkIrOpcode_Count,
} NkIrOpcode;

char const *nkirOpcodeName(u8 code);

typedef enum {
    NkIrRef_None = 0,
    NkIrRef_Frame,
    NkIrRef_Arg,
    NkIrRef_Ret,
    NkIrRef_Data,

    NkIrRef_Proc,
    NkIrRef_ExternData,
    NkIrRef_ExternProc,
    NkIrRef_Address,
    NkIrRef_VariadicMarker,
} NkIrRefKind;

typedef struct {
    usize index;
    usize offset;
    usize post_offset;
    nktype_t type;
    NkIrRefKind kind;
    u8 indir;
} NkIrRef;

typedef struct {
    NkIrRef const *data;
    usize size;
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
        usize id;
        NkString comment;
    };
    NkIrArgKind kind;
} NkIrArg;

typedef struct {
    NkIrArg arg[3];
    u32 line;
    u8 code;
} NkIrInstr;

#define DEFINE_IDX_TYPE(NAME) \
    typedef struct {          \
        usize idx;            \
    } NAME

DEFINE_IDX_TYPE(NkIrProc);
DEFINE_IDX_TYPE(NkIrLabel);
DEFINE_IDX_TYPE(NkIrLocalVar);
DEFINE_IDX_TYPE(NkIrData);
DEFINE_IDX_TYPE(NkIrExternData);
DEFINE_IDX_TYPE(NkIrExternProc);

#undef DEFINE_IDX_TYPE

#define NKIR_INVALID_IDX ((usize) - 1ul)

typedef struct NkIrModule_T *NkIrModule;
typedef struct NkIrProg_T *NkIrProg;

NkIrProg nkir_createProgram(NkArena *arena);

NkString nkir_getErrorString(NkIrProg ir);

// Modules

NkIrModule nkir_createModule(NkIrProg ir);

void nkir_exportProc(NkIrProg ir, NkIrModule mod, NkIrProc proc);
void nkir_exportData(NkIrProg ir, NkIrModule mod, NkIrData data);

void nkir_mergeModules(NkIrModule dst, NkIrModule src);

// Code Generation

NkIrProc nkir_createProc(NkIrProg ir);
NkIrLabel nkir_createLabel(NkIrProg ir, NkAtom name);

typedef struct {
    NkAtom name;
    nktype_t proc_t;
    NkAtomStridedArray arg_names;
    NkAtom file;
    usize line;
    NkIrVisibility visibility;
} NkIrProcDescr;

void nkir_startProc(NkIrProg ir, NkIrProc proc, NkIrProcDescr descr);

void nkir_setActiveProc(NkIrProg ir, NkIrProc proc);
NkIrProc nkir_getActiveProc(NkIrProg ir);

void nkir_finishProc(NkIrProg ir, NkIrProc proc, usize line);

void *nkir_getDataPtr(NkIrProg ir, NkIrData var);
void *nkir_dataRefDeref(NkIrProg ir, NkIrRef ref);
bool nkir_dataIsReadOnly(NkIrProg ir, NkIrData var);

nktype_t nkir_getProcType(NkIrProg ir, NkIrProc proc);
nktype_t nkir_getLocalType(NkIrProg ir, NkIrLocalVar var);
nktype_t nkir_getArgType(NkIrProg ir, usize idx);
nktype_t nkir_getDataType(NkIrProg ir, NkIrData var);
nktype_t nkir_getExternDataType(NkIrProg ir, NkIrExternData data);
nktype_t nkir_getExternProcType(NkIrProg ir, NkIrExternProc proc);

typedef NkSlice(NkIrInstr const) NkIrInstrArray;
typedef NkDynArray(NkIrInstr) NkIrInstrDynArray;

void nkir_emit(NkIrProg ir, NkIrInstr instr);
void nkir_emitArray(NkIrProg ir, NkIrInstrArray instrs);

void nkir_instrArrayDupInto(NkIrProg ir, NkIrInstrArray instrs, NkIrInstrDynArray *out, NkArena *tmp_arena);

void nkir_setLine(NkIrProg ir, usize line);
usize nkir_getLine(NkIrProg ir);

void nkir_enter(NkIrProg ir);
void nkir_leave(NkIrProg ir);

// References

NkIrLocalVar nkir_makeLocalVar(NkIrProg ir, NkAtom name, nktype_t type);
NkIrData nkir_makeData(NkIrProg ir, NkAtom name, nktype_t type, NkIrVisibility vis);
NkIrData nkir_makeRodata(NkIrProg ir, NkAtom name, nktype_t type, NkIrVisibility vis);
NkIrExternData nkir_makeExternData(NkIrProg ir, NkAtom lib, NkAtom name, nktype_t type);
NkIrExternProc nkir_makeExternProc(NkIrProg ir, NkAtom lib, NkAtom name, nktype_t proc_t);

NkIrRef nkir_makeFrameRef(NkIrProg ir, NkIrLocalVar var);
NkIrRef nkir_makeArgRef(NkIrProg ir, usize idx);
NkIrRef nkir_makeRetRef(NkIrProg ir);
NkIrRef nkir_makeDataRef(NkIrProg ir, NkIrData var);
NkIrRef nkir_makeProcRef(NkIrProg ir, NkIrProc proc);
NkIrRef nkir_makeExternDataRef(NkIrProg ir, NkIrExternData data);
NkIrRef nkir_makeExternProcRef(NkIrProg ir, NkIrExternProc proc);

NkIrRef nkir_makeAddressRef(NkIrProg ir, NkIrRef ref, nktype_t ptr_t);

NkIrRef nkir_makeVariadicMarkerRef(NkIrProg ir);

// Instructions

NkIrInstr nkir_make_nop(NkIrProg ir);
NkIrInstr nkir_make_ret(NkIrProg ir);

NkIrInstr nkir_make_jmp(NkIrProg ir, NkIrLabel label);
NkIrInstr nkir_make_jmpz(NkIrProg ir, NkIrRef cond, NkIrLabel label);
NkIrInstr nkir_make_jmpnz(NkIrProg ir, NkIrRef cond, NkIrLabel label);

NkIrInstr nkir_make_call(NkIrProg ir, NkIrRef dst, NkIrRef proc, NkIrRefArray args);

#define UNA_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef arg);
#define BIN_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#define DBL_IR(NAME1, NAME2) \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(NkIrProg ir, NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nkb/ir.inl"

NkIrInstr nkir_make_syscall(NkIrProg ir, NkIrRef dst, NkIrRef n, NkIrRefArray args);

NkIrInstr nkir_make_label(NkIrLabel label);

NkIrInstr nkir_make_comment(NkIrProg ir, NkString comment);

// Output

typedef struct {
    NkString compiler_binary;
    NkSlice(NkString const) additional_flags;
    NkString output_filename;
    NkbOutputKind output_kind;
    bool quiet;
} NkIrCompilerConfig;

bool nkir_write(NkIrProg ir, NkIrModule mod, NkArena *tmp_arena, NkIrCompilerConfig conf);

// Execution

typedef struct NkIrRunCtx_T *NkIrRunCtx;

NkIrRunCtx nkir_createRunCtx(NkIrProg ir, NkArena *tmp_arena);
void nkir_freeRunCtx(NkIrRunCtx ctx);

bool nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, void **args, void **ret);

NkString nkir_getRunErrorString(NkIrRunCtx ctx);

// Inspection

void nkir_inspectProgram(NkIrProg ir, NkStream out);
void nkir_inspectData(NkIrProg ir, NkStream out);
void nkir_inspectExternSyms(NkIrProg ir, NkStream out);
void nkir_inspectProc(NkIrProg ir, NkIrProc proc, NkStream out);
void nkir_inspectInstr(NkIrProg ir, NkIrProc proc, NkIrInstr instr, NkStream out);
void nkir_inspectRef(NkIrProg ir, NkIrProc proc, NkIrRef ref, NkStream out);

#ifdef __cplusplus
}
#endif

#endif // NKB_IR_H_
