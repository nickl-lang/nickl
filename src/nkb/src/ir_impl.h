#ifndef NKB_IR_IMPL_H_
#define NKB_IR_IMPL_H_

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkAtom name;
    nktype_t type;
    usize offset;
} NkIrLocal_T;

typedef struct {
    NkDynArray(usize) blocks;
    NkDynArray(NkIrLocal_T) locals;
    NkDynArray(usize) scopes;

    NkAtom name{};
    nktype_t proc_t{};
    NkAtomArray arg_names{};
    NkIrVisibility visibility{};

    NkAtom file{};
    usize start_line{};
    usize end_line{};

    usize cur_block{};

    usize frame_size{};
    usize frame_align{1};
    usize cur_frame_size{};
} NkIrProc_T;

typedef struct {
    NkAtom name;
    NkDynArray(usize) instrs;
} NkIrBlock;

typedef struct {
    NkAtom name;
    void *data;
    nktype_t type;
    NkIrVisibility visibility;
    bool read_only;
} NkIrDecl_T;

typedef struct {
    NkAtom lib;
    NkAtom name;
    nktype_t type;
} NkIrExternSym_T;

struct NkIrProg_T {
    NkAllocator alloc;

    NkDynArray(NkIrProc_T) procs;
    NkDynArray(NkIrBlock) blocks;
    NkDynArray(NkIrInstr) instrs;
    NkDynArray(NkIrDecl_T) data;
    NkDynArray(NkIrExternSym_T) extern_data;
    NkDynArray(NkIrExternSym_T) extern_procs;
    NkDynArray(NkIrRef) relocs;

    NkIrProc cur_proc{};
    usize cur_line{};

    NkString error_str{};
};

#ifdef __cplusplus
}
#endif

#endif // NKB_IR_IMPL_H_