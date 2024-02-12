#ifndef NKB_IR_IMPL_HPP_
#define NKB_IR_IMPL_HPP_

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"

struct NkIrLocal_T {
    NkAtom name;
    nktype_t type;
    usize offset;
};

struct NkIrProc_T {
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
};

struct NkIrBlock {
    NkAtom name;
    NkDynArray(usize) instrs;
};

struct NkIrDecl_T {
    NkAtom name;
    void *data;
    nktype_t type;
    NkIrVisibility visibility;
    bool read_only;
};

struct NkIrExternSym_T {
    NkAtom lib;
    NkAtom name;
    nktype_t type;
};

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

#endif // NKB_IR_IMPL_HPP_
