#ifndef HEADER_GUARD_NKB_IR_IMPL
#define HEADER_GUARD_NKB_IR_IMPL

#include <cstddef>

#include "nkb/ir.h"
#include "ntk/array.h"
#include "ntk/string.h"

nkar_typedef(size_t, nkar_size_t);
nkar_typedef(nktype_t, nkar_nktype_t);

struct NkIrProc_T {
    nkar_size_t blocks;
    nkar_nktype_t locals;

    nkstr name{};
    nktype_t proc_t{};

    size_t cur_block{};
};

struct NkIrBlock {
    nkstr name;
    nkar_size_t instrs;
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

nkar_typedef(NkIrProc_T, nkar_NkIrProc_T);
nkar_typedef(NkIrBlock, nkar_NkIrBlock);
nkar_typedef(NkIrInstr, nkar_NkIrInstr);
nkar_typedef(NkIrConst_T, nkar_NkIrConst_T);
nkar_typedef(NkIrExternData_T, nkar_NkIrExternData_T);
nkar_typedef(NkIrExternProc_T, nkar_NkIrExternProc_T);
nkar_typedef(NkIrRef, nkar_NkIrRef);

struct NkIrProg_T {
    NkAllocator alloc;

    nkar_NkIrProc_T procs;
    nkar_NkIrBlock blocks;
    nkar_NkIrInstr instrs;
    nkar_nktype_t globals;
    nkar_NkIrConst_T consts;
    nkar_NkIrExternData_T extern_data;
    nkar_NkIrExternProc_T extern_procs;
    nkar_NkIrRef relocs;

    NkIrProc cur_proc{};
};

#endif // HEADER_GUARD_NKB_IR_IMPL
