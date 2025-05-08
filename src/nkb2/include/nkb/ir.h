#ifndef NKB_IR_H_
#define NKB_IR_H_

#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Instrs

enum {
#define IR(NAME) NK_CAT(NkIrOp_, NAME),
#include "nkb/ir.inl"
};

/// Common

typedef enum {
    NkIrRef_None = 0,

    NkIrRef_Null,
    NkIrRef_Local,
    NkIrRef_Param,
    NkIrRef_Global,
    NkIrRef_Imm,

    NkIrRef_VariadicMarker,
} NkIrRefKind;

typedef union {
    i8 i8;
    u8 u8;
    i16 i16;
    u16 u16;
    i32 i32;
    u32 u32;
    i64 i64;
    u64 u64;
    f32 f32;
    f64 f64;
} NkIrImm;

typedef struct {
    union {
        NkAtom sym;  // Local, Param, Global
        NkIrImm imm; // Imm
    };
    NkIrType type;
    NkIrRefKind kind;
} NkIrRef;

typedef enum {
    NkIrArg_None = 0,

    NkIrArg_Ref,
    NkIrArg_RefArray,
    NkIrArg_Label,
    NkIrArg_RelLabel,
    NkIrArg_Type, // TODO: Replace type with 2 imms for size and align?
    NkIrArg_String,
    NkIrArg_Idx,
} NkIrArgKind;

typedef NkSlice(NkIrRef const) NkIrRefArray;

typedef struct {
    union {
        NkIrRef ref;       // Ref
        NkIrRefArray refs; // RefArray
        struct {
            NkAtom name; // Label
            i32 offset;  // RelLabel
        } label;
        NkIrType type; // Type
        NkString str;  // String
        u32 idx;       // Idx
    };
    NkIrArgKind kind;
} NkIrArg;

typedef struct {
    NkIrArg arg[3];
    u8 code;
} NkIrInstr;

typedef NkSlice(NkIrInstr const) NkIrInstrArray;
typedef NkDynArray(NkIrInstr) NkIrInstrDynArray;

typedef enum {
    NkIrSymbol_Extern,
    NkIrSymbol_Data,
    NkIrSymbol_Proc,
} NkIrSymbolKind;

// TODO: NkIrExtern will probably only be used in bytecode translation, think whether we need it as a symbol.
typedef struct {
    NkIrType type;
    void *addr;
} NkIrExtern;

typedef struct {
    NkAtom sym;
    usize offset;
} NkIrReloc;

typedef NkSlice(NkIrReloc const) NkIrRelocArray;

typedef enum {
    NkIrData_ReadOnly = 1 << 0,
} NkIrDataFlags;

typedef struct {
    NkIrType type;
    NkIrRelocArray relocs;
    void *addr;
    NkIrDataFlags flags;
} NkIrData;

typedef struct {
    NkAtom name;
    NkIrType type;
} NkIrParam;

typedef NkSlice(NkIrParam const) NkIrParamArray;

typedef enum {
    NkIrProc_Variadic = 1 << 0,
} NkIrProcFlags;

typedef struct {
    NkIrParamArray params;
    NkIrType ret_type;
    NkIrInstrArray instrs;
    NkString file;
    u32 line;
    NkIrProcFlags flags;
} NkIrProc;

typedef enum {
    NkIrVisibility_Hidden = 0,
    NkIrVisibility_Default,
    NkIrVisibility_Protected,
    NkIrVisibility_Internal,
    NkIrVisibility_Local,
} NkIrVisibility;

typedef enum {
    NkIrSymbol_ThreadLocal = 1 << 0,
} NkIrSymbolFlags;

typedef struct {
    union {
        NkIrExtern extrn; // Extern
        NkIrData data;    // Data
        NkIrProc proc;    // Proc
    };
    NkAtom name;
    NkIrVisibility vis;
    NkIrSymbolFlags flags;
    NkIrSymbolKind kind;
} NkIrSymbol;

typedef NkSlice(NkIrSymbol const) NkIrSymbolArray;
typedef NkDynArray(NkIrSymbol) NkIrSymbolDynArray;

typedef NkIrSymbolArray NkIrModule;

/// Main

void nkir_convertToPic(NkIrInstrArray instrs, NkIrInstrDynArray *out);

/// Refs

NkIrRef nkir_makeRefNull(NkIrType type);
NkIrRef nkir_makeRefLocal(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefParam(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefGlobal(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefImm(NkIrImm imm, NkIrType type);

NkIrRef nkir_makeVariadicMarker();

/// Codegen

NkIrInstr nkir_make_nop();

NkIrInstr nkir_make_ret(NkIrRef arg);

NkIrInstr nkir_make_jmp(NkAtom label);
NkIrInstr nkir_make_jmpz(NkIrRef cond, NkAtom label);
NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkAtom label);

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args);

NkIrInstr nkir_make_store(NkIrRef ptr, NkIrRef src);
NkIrInstr nkir_make_load(NkIrRef dst, NkIrRef ptr);

NkIrInstr nkir_make_alloc(NkIrRef ptr, NkIrType type);

#define UNA_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg);
#define BIN_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#define DBL_IR(NAME1, NAME2) \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nkb/ir.inl"

NkIrInstr nkir_make_label(NkAtom label);

NkIrInstr nkir_make_file(NkString file);
NkIrInstr nkir_make_line(u32 line);
NkIrInstr nkir_make_comment(NkString comment);

/// Output

void nkir_exportModule(NkIrModule m, NkString path /*, c_compiler_config */);

/// Execution

typedef struct NkIrRunCtx_T *NkIrRunCtx;

NkIrRunCtx nkir_createRunCtx(NkIrModule *m, NkArena *arena);

bool nkir_invoke(NkIrRunCtx ctx, NkAtom sym, void **args, void **ret);

/// Inspection

void nkir_inspectModule(NkIrModule m, NkStream out);
void nkir_inspectSymbol(NkIrSymbol const *sym, NkStream out);
void nkir_inspectInstr(NkIrInstr instr, NkStream out);
void nkir_inspectRef(NkIrRef ref, NkStream out);

/// Validation

bool nkir_validateModule(NkIrModule m);
bool nkir_validateProc(NkIrProc const *proc);

#ifdef __cplusplus
}
#endif

#endif // NKB_IR_H_
