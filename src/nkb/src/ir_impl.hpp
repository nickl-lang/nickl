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
    NkIrLine start_line{};
    NkIrLine end_line{};

    size_t cur_block{};
    NkIrLine last_line{};
};

struct NkIrBlock {
    nkid name;
    nkar_type(size_t) instrs;
};

struct NkIrConst_T {
    nkid name;
    void *data;
    nktype_t type;
};

struct NkIrProg_T {
    NkAllocator alloc;

    nkar_type(NkIrProc_T) procs;
    nkar_type(NkIrBlock) blocks;
    nkar_type(NkIrInstr) instrs;
    nkar_type(NkIrDecl_T) globals;
    nkar_type(NkIrConst_T) consts;
    nkar_type(NkIrDecl_T) extern_data;
    nkar_type(NkIrDecl_T) extern_procs;
    nkar_type(NkIrRef) relocs;

    NkIrProc cur_proc{};
};

#endif // HEADER_GUARD_NKB_IR_IMPL
