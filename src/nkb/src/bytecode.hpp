#ifndef HEADER_GUARD_NKB_BYTECODE
#define HEADER_GUARD_NKB_BYTECODE

#include <cstddef>

#include "nk/common/allocator.h"
#include "nk/common/array.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"

enum NkBcOpcode {
#define OP(NAME) CAT(nkop_, NAME),
#include "bytecode.inl"

    NkBcOpcode_Count,
};

char const *nkbcOpcodeName(uint16_t code);

enum NkBcRefKind { // must preserve NkIrRefKind order
    NkBcRef_None = 0,
    NkBcRef_Frame,
    NkBcRef_Arg,
    NkBcRef_Ret,

    NkBcRef_Rodata,
    NkBcRef_Data,
    NkBcRef_Instr,

    NkBcRef_Count,
};

struct NkBcRef {
    size_t offset;
    nktype_t type;
    NkBcRefKind kind;
    bool is_indirect;
};

struct NkBcInstr {
    NkBcRef arg[3];
    uint16_t code;
};

typedef struct NkBcProg_T *NkBcProg;
typedef struct NkBcProc_T *NkBcProc;

struct NkBcProc_T {
    NkBcProg prog;
    size_t frame_size;
    NkBcInstr *instrs;
};

struct NkIrRunCtx_T {
    NkIrProg ir;

    NkArray<NkBcProc> procs;
    NkArray<NkArray<NkBcInstr>> instrs;
};

#endif // HEADER_GUARD_NKB_BYTECODE
