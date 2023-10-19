#ifndef HEADER_GUARD_NKB_IR_IMPL
#define HEADER_GUARD_NKB_IR_IMPL

#include <cstddef>

#include "nkb/ir.h"
#include "ntk/array.h"
#include "ntk/id.h"

struct NkIrDecl_T {
    nkid name;
    nktype_t type;
};

struct NkIrProc_T {
    nkar_type(size_t) blocks;
    nkar_type(NkIrDecl_T) locals;

    nkid name{};
    nktype_t proc_t{};
    nkid_array arg_names{};
    NkIrVisibility visibility{};

    nkid file{};
    size_t start_line{};
    size_t end_line{};

    size_t cur_block{};
};

struct NkIrBlock {
    nkid name;
    nkar_type(size_t) instrs;
};

struct NkIrConst_T {
    nkid name;
    void *data;
    nktype_t type;
    NkIrVisibility visibility;
};

struct NkIrGlobal_T {
    nkid name;
    nktype_t type;
    NkIrVisibility visibility;
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
    nkar_type(NkIrGlobal_T) globals;
    nkar_type(NkIrConst_T) consts;
    nkar_type(NkIrExternSym_T) extern_data;
    nkar_type(NkIrExternSym_T) extern_procs;
    nkar_type(NkIrRef) relocs;

    NkIrProc cur_proc{};
    size_t cur_line{};

    nks error_str{};
};

#endif // HEADER_GUARD_NKB_IR_IMPL
