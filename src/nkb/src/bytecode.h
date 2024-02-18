#ifndef NKB_BYTECODE_H_
#define NKB_BYTECODE_H_

#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_map.hpp"
#include "ntk/os/common.h"
#include "ntk/os/dl.h"
#include "ntk/os/thread.h"

#ifdef __cplusplus
extern "C" {
#endif

enum NkBcOpcode {
#define OP(NAME) NK_CAT(nkop_, NAME),
#include "bytecode.inl"

    NkBcOpcode_Count,
};

char const *nkbcOpcodeName(u16 code);

enum NkBcRefKind { // must preserve NkIrRefKind order
    NkBcRef_None = 0,
    NkBcRef_Frame,
    NkBcRef_Arg,
    NkBcRef_Ret,
    NkBcRef_Data,

    NkBcRef_Instr,

    NkBcRef_Count,
};

struct NkBcRef {
    usize offset;
    usize post_offset;
    nktype_t type;
    NkBcRefKind kind;
    u8 indir;
};

struct NkBcRefArray {
    NkBcRef *data;
    usize size;
};

typedef enum {
    NkBcArg_None,

    NkBcArg_Ref,
    NkBcArg_RefArray,
} NkBcArgKind;

struct NkBcArg {
    union {
        NkBcRef ref;
        NkBcRefArray refs;
    };
    NkBcArgKind kind;
};

struct NkBcInstr {
    NkBcArg arg[3];
    u16 code;
};

typedef struct NkBcProc_T *NkBcProc;

struct NkBcProc_T {
    NkIrRunCtx ctx;
    usize frame_size;
    usize frame_align;
    NkDynArray(NkBcInstr) instrs;
};

struct NkFfiContext {
    NkAllocator alloc;
    NkHashMap<u64, void *> typemap{};
    NkOsHandle mtx{};
};

struct NkIrRunCtx_T {
    NkIrProg ir;
    NkArena *tmp_arena;

    NkDynArray(NkBcProc) procs;
    NkDynArray(void *) data;
    NkHashMap<NkAtom, NkOsHandle> extern_libs;
    NkHashMap<NkAtom, void *> extern_syms;

    NkFfiContext ffi_ctx;

    NkString error_str{};
};

NK_INLINE void *nkbc_deref(u8 *base, NkBcRef const &ref) {
    u8 *ptr = base + ref.offset;
    int indir = ref.indir;
    while (indir--) {
        ptr = *(u8 **)ptr;
    }
    return ptr + ref.post_offset;
}

#ifdef __cplusplus
}
#endif

#endif // NKB_BYTECODE_H_
