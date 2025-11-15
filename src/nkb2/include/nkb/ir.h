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

typedef enum {
#define IR(NAME) NK_CAT(NkIrOp_, NAME),
#include "nkb/ir.inl"
} NkIrOpcode;

/// Types

typedef struct NkIrSymbol NkIrSymbol;

typedef NkSlice(NkIrSymbol const) NkIrSymbolArray;
typedef NkDynArray(NkIrSymbol) NkIrSymbolDynArray;

NK_HASH_TREE_ARRAY_PROTO(NkIrSymbolDynArray, NkIrSymbol, NkAtom);

typedef struct NkbState_T *NkbState;
typedef NkIrSymbolDynArray const *NkIrModule;
typedef struct NkIrTarget_T *NkIrTarget;

typedef struct NkIrRuntime_T *NkIrRuntime;
typedef struct NkIrDylib_T *NkIrDylib;

typedef enum {
    NkIrOutput_None = 0,

    NkIrOutput_Binary,
    NkIrOutput_Static,
    NkIrOutput_Shared,
    NkIrOutput_Archiv,
    NkIrOutput_Object,
} NkIrOutputKind;

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
        NkAtom sym;  // NkIrRef_Local, NkIrRef_Param, NkIrRef_Global
        NkIrImm imm; // NkIrRef_Imm
    };
    NkIrType type;
    NkIrRefKind kind;
} NkIrRef;

typedef enum {
    NkIrArg_None = 0,

    NkIrArg_Ref,
    NkIrArg_RefArray,
    NkIrArg_Label,
    NkIrArg_LabelRel,
    NkIrArg_Type,
    NkIrArg_String,
    NkIrArg_PhiArgsArray,
} NkIrArgKind;

typedef NkSlice(NkIrRef const) NkIrRefArray;
typedef NkDynArray(NkIrRef) NkIrRefDynArray;

typedef struct {
    NkIrRef ref;
    NkAtom label;
} NkIrPhiArg;

typedef NkSlice(NkIrPhiArg const) NkIrPhiArgArray;
typedef NkDynArray(NkIrPhiArg) NkIrPhiArgDynArray;

typedef struct {
    union {
        NkIrRef ref;              // NkIrArg_Ref
        NkIrRefArray refs;        // NkIrArg_RefArray
        NkAtom label;             // NkIrArg_Label
        i32 offset;               // NkIrArg_LabelRel
        NkIrType type;            // NkIrArg_Type
        NkString str;             // NkIrArg_String
        NkIrPhiArgArray phi_args; // NkIrArg_PhiArgsArray
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
    NkIrSymbol_None = 0,

    NkIrSymbol_Proc,
    NkIrSymbol_Data,
    NkIrSymbol_Extern,
} NkIrSymbolKind;

typedef enum {
    NkIrProc_Variadic = 1 << 0,
} NkIrProcFlags;

typedef NkSlice(NkIrType const) NkIrTypeArray;
typedef NkDynArray(NkIrType) NkIrTypeDynArray;

typedef struct {
    NkAtom sym;
    usize offset;
} NkIrReloc;

typedef NkSlice(NkIrReloc const) NkIrRelocArray;
typedef NkDynArray(NkIrReloc) NkIrRelocDynArray;

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
typedef NkDynArray(NkIrParam) NkIrParamDynArray;

typedef struct {
    NkIrParamArray params;
    NkIrParam ret;
    NkIrInstrArray instrs;
    NkIrProcFlags flags;
} NkIrProc;

typedef enum {
    NkIrVisibility_Unknown = 0,

    NkIrVisibility_Default,
    NkIrVisibility_Hidden,
    NkIrVisibility_Protected,
    NkIrVisibility_Internal,
    NkIrVisibility_Local,
} NkIrVisibility;

typedef enum {
    NkIrSymbol_ThreadLocal = 1 << 0,
} NkIrSymbolFlags;

typedef enum {
    NkIrExtern_Proc,
    NkIrExtern_Data,
} NkIrExternKind;

typedef struct {
    union {
        struct {
            NkIrTypeArray param_types;
            NkIrType ret_type;
            NkIrProcFlags flags;
        } proc; // NkIrExtern_Proc
        struct {
            NkIrType type;
        } data; // NkIrExtern_Data
    };
    NkAtom lib;
    NkIrExternKind kind;
} NkIrExtern;

struct NkIrSymbol {
    union {
        NkIrProc proc;    // NkIrSymbol_Proc
        NkIrData data;    // NkIrSymbol_Data
        NkIrExtern extrn; // NkIrSymbol_Extern
    };
    NkAtom name;
    NkIrVisibility vis;
    NkIrSymbolFlags flags;
    NkIrSymbolKind kind;

    usize left;
    usize right;
};

typedef enum {
    NkIrLabel_Abs,
    NkIrLabel_Rel,
} NkIrLabelKind;

typedef struct {
    union {
        NkAtom name; // NkIrLabel_Abs
        i32 offset;  // NkIrLabel_Rel
    };
    NkIrLabelKind kind;
} NkIrLabel;

typedef struct {
    NkAtom sym;
    void *addr;
} NkIrSymbolAddress;

typedef NkSlice(NkIrSymbolAddress const) NkIrSymbolAddressArray;
typedef NkDynArray(NkIrSymbolAddress) NkIrSymbolAddressDynArray;

/// Main

NkbState nkir_createState(NkArena *arena);
void nkir_freeState(NkbState nkb);

NkIrTarget nkir_createTarget(NkbState nkb, NkString triple);
void nkir_freeTarget(NkIrTarget tgt);

/// Utility

void nkir_convertToPic(NkIrInstrArray instrs, NkIrInstrDynArray *out);

/// Refs

NkIrRef nkir_makeRefNull(NkIrType type);
NkIrRef nkir_makeRefLocal(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefParam(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefGlobal(NkAtom sym, NkIrType type);
NkIrRef nkir_makeRefImm(NkIrImm imm, NkIrType type);

NkIrRef nkir_makeVariadicMarker(void);

/// Labels

NkIrLabel nkir_makeLabelAbs(NkAtom name);
NkIrLabel nkir_makeLabelRel(i32 offset);

/// Codegen

NkIrInstr nkir_make_nop(void);

NkIrInstr nkir_make_ret(NkIrRef arg);

NkIrInstr nkir_make_jmp(NkIrLabel label);
NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label);
NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label);

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args);

NkIrInstr nkir_make_offset(NkIrRef dst, NkIrRef ptr, NkIrRef idx);
NkIrInstr nkir_make_store(NkIrRef dst, NkIrRef src);
NkIrInstr nkir_make_load(NkIrRef dst, NkIrRef ptr);

NkIrInstr nkir_make_alloc(NkIrRef dst, NkIrType type);

#define UNA_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg);
#define BIN_IR(NAME) NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#define DBL_IR(NAME1, NAME2) \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs);
#include "nkb/ir.inl"

NkIrInstr nkir_make_phi(NkIrRef dst, NkIrPhiArgArray args);

NkIrInstr nkir_make_label(NkAtom label);

NkIrInstr nkir_make_comment(NkString comment);

/// Output

bool nkir_exportModule(NkbState nkb, NkIrModule mod, NkIrTarget tgt, NkString out_file, NkIrOutputKind kind);

/// Runtime

NkIrRuntime nkir_createRuntime(NkArena *arena, NkbState nkb);
void nkir_freeRuntime(NkIrRuntime rt);

NkIrDylib nkir_createDylib(NkArena *arena, NkbState nkb, NkIrRuntime rt, NkIrModule mod);

typedef void *(*NkIrSymbolResolver)(NkAtom sym, void *userdata);
void nkir_setSymbolResolver(NkIrDylib dl, NkIrSymbolResolver fn, void *userdata);

void *nkir_getSymbolAddress(NkbState nkb, NkIrDylib dl, NkAtom sym);
bool nkir_defineExternSymbols(NkIrDylib dl, NkIrSymbolAddressArray syms);

bool nkir_invoke(NkIrDylib dl, NkAtom sym, void **args, void **ret);

/// Inspection

void nkir_printName(NkStream out, char const *kind, NkAtom name);
void nkir_printSymbolName(NkStream out, NkAtom sym);

void nkir_inspectModule(NkStream out, NkIrModule mod);
void nkir_inspectSymbol(NkStream out, NkIrSymbol const *sym);
void nkir_inspectInstr(NkStream out, NkIrInstr instr);
void nkir_inspectRef(NkStream out, NkIrRef ref);

/// Validation

bool nkir_validateModule(NkIrModule mod);
bool nkir_validateProc(NkIrProc const *proc);

#ifdef __cplusplus
}
#endif

#endif // NKB_IR_H_
