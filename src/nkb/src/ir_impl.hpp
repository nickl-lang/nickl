#ifndef HEADER_GUARD_NKB_IR_IMPL
#define HEADER_GUARD_NKB_IR_IMPL

#include <cstddef>

#include "nk/common/array.hpp"
#include "nk/common/string.h"
#include "nkb/ir.h"

struct NkIrProc_T {
    NkArray<size_t> blocks;
    NkArray<nktype_t> locals;

    nkstr name{};
    nktype_t proc_t{};

    size_t cur_block{};
};

struct NkIrBlock {
    nkstr name;
    NkArray<size_t> instrs;
};

struct NkIrConst_T {
    void *data;
    nktype_t type;
};

struct NkIrExternData_T {
    nkstr name;
    nktype_t type;
};

struct NkIrExternProc_T {
    nkstr name;
    nktype_t proc_t;
};

struct NkIrProg_T {
    NkAllocator alloc;

    NkArray<NkIrProc_T> procs;
    NkArray<NkIrBlock> blocks;
    NkArray<NkIrInstr> instrs;
    NkArray<nktype_t> globals;
    NkArray<NkIrConst_T> consts;
    NkArray<NkIrExternData_T> extern_data;
    NkArray<NkIrExternProc_T> extern_procs;
    NkArray<NkIrRef> relocs;

    NkIrProc cur_proc{};
};

#endif // HEADER_GUARD_NKB_IR_IMPL
