#ifndef HEADER_GUARD_NK_VM_BYTECODE_IMPL
#define HEADER_GUARD_NK_VM_BYTECODE_IMPL

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "bytecode.h"
#include "dl_adapter.h"
#include "native_fn_adapter.h"
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
    NkBcRef_Reg,

    NkBcRef_Abs,
    NkBcRef_Instr,

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

struct NkBcFunct_T {
    NkBcProg prog;
    size_t frame_size;
    NkBcInstr *instrs;
    nktype_t fn_t;
};

struct NkBcProg_T {
    NkIrProg ir;

    NkAllocator *arena;

    std::vector<nkval_t> globals{};
    std::deque<NkBcFunct_T> functs{};

    std::vector<std::vector<NkBcInstr>> instrs{};

    std::vector<NkDlHandle> shobjs{};
    std::deque<void *> exsyms{};

    std::vector<NkNativeClosure> closures{};
};

#endif // HEADER_GUARD_NK_VM_BYTECODE_IMPL
