#ifndef NKB_BYTECODE_H_
#define NKB_BYTECODE_H_

#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define OP(NAME) NK_CAT(nkop_, NAME),
#include "bytecode.inl"

    NkBcOpcode_Count,
} NkBcOpcode;

char const *nkbcOpcodeName(u16 code);

typedef enum {
    NkBcRef_None = 0,
    NkBcRef_Frame,
    NkBcRef_Arg,
    NkBcRef_Data,

    NkBcRef_Instr,
    NkBcRef_VariadicMarker,

    NkBcRef_Count,
} NkBcRefKind; // must preserve NkIrRefKind order

typedef struct {
    usize offset;
    usize post_offset;
    nktype_t type;
    NkBcRefKind kind;
    u8 indir;
} NkBcRef;

typedef struct {
    NkBcRef *data;
    usize size;
} NkBcRefArray;

typedef enum {
    NkBcArg_None,

    NkBcArg_Ref,
    NkBcArg_RefArray,
} NkBcArgKind;

typedef struct {
    union {
        NkBcRef ref;
        NkBcRefArray refs;
    };
    NkBcArgKind kind;
} NkBcArg;

typedef struct {
    NkBcArg arg[3];
    u16 code;
} NkBcInstr;

typedef struct NkBcProc_T *NkBcProc;

struct NkBcProc_T {
    NkIrRunCtx ctx;
    usize frame_size;
    usize frame_align;
    NkDynArray(NkBcInstr) instrs;
};

typedef struct {
    u32 key;
    void *val;
} TypeTree_kv;

// TODO: Use hash map for types
NK_HASH_TREE_TYPEDEF(TypeTree, TypeTree_kv);
NK_HASH_TREE_PROTO(TypeTree, TypeTree_kv, u32);

typedef struct {
    NkAllocator alloc;
    TypeTree types;
    NkHandle mtx;
} NkFfiContext;

typedef struct {
    NkAtom key;
    void *val;
} ExternSym_kv;

// TODO: Use hash map or linear array for extern syms
NK_HASH_TREE_TYPEDEF(ExternSymTree, ExternSym_kv);
NK_HASH_TREE_PROTO(ExternSymTree, ExternSym_kv, NkAtom);

struct NkIrRunCtx_T {
    NkIrProg ir;
    NkArena *tmp_arena;

    NkDynArray(NkBcProc) procs;
    NkDynArray(void *) data;
    ExternSymTree extern_syms;

    NkFfiContext ffi_ctx;

    NkString error_str;
};

NK_INLINE void *nkbc_deref(u8 *base, NkBcRef const *ref) {
    u8 *ptr = base + ref->offset;
    int indir = ref->indir;
    while (indir--) {
        ptr = *(u8 **)ptr;
    }
    return ptr + ref->post_offset;
}

#ifdef __cplusplus
}
#endif

#endif // NKB_BYTECODE_H_
