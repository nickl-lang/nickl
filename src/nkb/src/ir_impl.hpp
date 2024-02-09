#ifndef HEADER_GUARD_NKB_IR_IMPL
#define HEADER_GUARD_NKB_IR_IMPL

#include <cstddef>

#include "nkb/ir.h"
#include "ntk/array.h"
#include "ntk/id.h"

struct NkIrLocal_T {
    nkid name;
    nktype_t type;
    usize offset;
};

struct NkIrProc_T {
    nkar_type(usize) blocks;
    nkar_type(NkIrLocal_T) locals;
    nkar_type(usize) scopes;

    nkid name{};
    nktype_t proc_t{};
    nkid_array arg_names{};
    NkIrVisibility visibility{};

    nkid file{};
    usize start_line{};
    usize end_line{};

    usize cur_block{};

    usize frame_size{};
    usize frame_align{1};
    usize cur_frame_size{};
};

struct NkIrBlock {
    nkid name;
    nkar_type(usize) instrs;
};

struct NkIrDecl_T {
    nkid name;
    void *data;
    nktype_t type;
    NkIrVisibility visibility;
    bool read_only;
};

struct NkIrExternSym_T {
    nkid lib;
    nkid name;
    nktype_t type;
};

struct NkIrProg_T {
    NkAllocator alloc;

    nkar_type(NkIrProc_T) procs;
    nkar_type(NkIrBlock) blocks;
    nkar_type(NkIrInstr) instrs;
    nkar_type(NkIrDecl_T) data;
    nkar_type(NkIrExternSym_T) extern_data;
    nkar_type(NkIrExternSym_T) extern_procs;
    nkar_type(NkIrRef) relocs;

    NkIrProc cur_proc{};
    usize cur_line{};

    nks error_str{};
};

#endif // HEADER_GUARD_NKB_IR_IMPL
