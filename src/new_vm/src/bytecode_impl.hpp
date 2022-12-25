#ifndef HEADER_GUARD_NK_VM_BYTECODE_IMPL
#define HEADER_GUARD_NK_VM_BYTECODE_IMPL

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include "bytecode.h"
#include "dl_adapter.h"
#include "nk/common/allocator.h"
#include "nk/vm/ir.h"

typedef enum {
#define X(NAME) CAT(nkop_, NAME),
#include "bytecode.inl"

    nkop_count,
} NkOpCode;

extern char const *s_nk_bc_names[];

typedef enum { // must preserve NkIrRefType order
    NkBcRef_None,
    NkBcRef_Frame,
    NkBcRef_Arg,
    NkBcRef_Ret,
    NkBcRef_Global,
    NkBcRef_Const,
    NkBcRef_Reg,

    NkBcRef_Instr,
    NkBcRef_Abs,

    NkBcRef_Count,
} NkBcRefType;

typedef struct {
    size_t offset;
    size_t post_offset;
    nktype_t type;
    NkBcRefType ref_type;
    bool is_indirect;
} NkBcRef;

typedef struct {
    NkBcRef arg[3];
    uint16_t code;
} NkBcInstr;

struct BytecodeFunct {
    NkBcProg prog;
    size_t frame_size;
    size_t first_instr;
    nktype_t fn_t;
};

struct NkBcProg_T {
    NkIrProg ir;

    // TODO std::unordered_map<size_t, nkval_t> globals;
    std::unordered_map<NkIrFunct, BytecodeFunct> functs;

    std::vector<NkBcInstr> instrs;

    std::vector<NkDlHandle> shobjs;
    std::deque<void *> exsyms;
};

#endif // HEADER_GUARD_NK_VM_BYTECODE_IMPL