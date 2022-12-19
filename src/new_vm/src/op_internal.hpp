#ifndef HEADER_GUARD_NK_VM_OP_INTERNAL
#define HEADER_GUARD_NK_VM_OP_INTERNAL

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "nk/common/allocator.h"
#include "op.h"

struct FunctInfo {
    NkOpProg prog;
    size_t frame_size;
    size_t frame_align;
    size_t first_instr;
    size_t instr_count;
    nktype_t fn_t;
};

struct NkOpProg_T {
    NkIrProg ir;

    std::unordered_map<size_t, nkval_t> globals;
    std::unordered_map<size_t, FunctInfo> functs;

    std::vector<NkOpInstr> instrs;

    NkAllocator *arena;
};

#endif // HEADER_GUARD_NK_VM_OP_INTERNAL
