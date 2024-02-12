#ifndef NK_VM_BYTECODE_IMPL_HPP_
#define NK_VM_BYTECODE_IMPL_HPP_

#include <deque>
#include <vector>

#include "bytecode.h"
#include "dl_adapter.h"
#include "nk/vm/ir.h"
#include "ntk/allocator.h"

typedef enum {
#define X(NAME) NK_CAT(nkop_, NAME),
#include "bytecode.inl"

    nkop_count,
} NkOpCode;

extern char const *s_nk_bc_names[];

typedef enum { // must preserve NkIrRefType order
    NkBcRef_None,
    NkBcRef_Frame,
    NkBcRef_Arg,
    NkBcRef_Ret,
    NkBcRef_Reg,

    NkBcRef_Rodata,
    NkBcRef_Data,
    NkBcRef_Instr,

    NkBcRef_Count,
} NkBcRefType;

typedef struct {
    usize offset;
    usize post_offset;
    nktype_t type;
    NkBcRefType ref_type;
    bool is_indirect;
} NkBcRef;

typedef struct {
    NkBcRef arg[3];
    u16 code;
} NkBcInstr;

struct NkBcFunct_T {
    NkBcProg prog;
    usize frame_size;
    NkBcInstr *instrs;
    nktype_t fn_t;
};

struct NkBcProg_T {
    NkIrProg ir;

    NkArena arena{};

    std::vector<nkval_t> globals{};
    std::deque<NkBcFunct_T> functs{};

    std::vector<std::vector<NkBcInstr>> instrs{};

    std::vector<NkDlHandle> shobjs{};
    std::deque<void *> exsyms{};
};

#endif // NK_VM_BYTECODE_IMPL_HPP_
