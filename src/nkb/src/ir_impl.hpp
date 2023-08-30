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
    NkIrProcInfo proc_info{};

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

struct NkIrProg_T {
    NkAllocator alloc;

    NkIrProc cur_proc;

    NkArray<NkIrProc_T> procs;
    NkArray<NkIrBlock> blocks;
    NkArray<NkIrInstr> instrs;
    NkArray<nktype_t> globals;
    NkArray<NkIrConst_T> consts;
};

#endif // HEADER_GUARD_NKB_IR_IMPL
