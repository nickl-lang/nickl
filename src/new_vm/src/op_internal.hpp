#ifndef HEADER_GUARD_NK_VM_OP_INTERNAL
#define HEADER_GUARD_NK_VM_OP_INTERNAL

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "nk/common/allocator.h"
#include "op.h"

typedef enum {
#define X(NAME) CAT(nkop_, NAME),
#include "op.inl"

    nkop_count,
} NkOpCode;

extern char const *s_nk_op_names[];

typedef enum { // must preserve NkIrRefType order
    NkOpRef_None,
    NkOpRef_Frame,
    NkOpRef_Arg,
    NkOpRef_Ret,
    NkOpRef_Global,
    NkOpRef_Const,
    NkOpRef_Reg,

    NkOpRef_Instr,
    NkOpRef_Abs,

    NkOpRef_Count,
} NkOpRefType;

typedef struct {
    size_t offset;
    size_t post_offset;
    nktype_t type;
    NkOpRefType ref_type;
    bool is_indirect;
} NkOpRef;

typedef struct {
    NkOpRef arg[3];
    uint16_t code;
} NkOpInstr;

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
