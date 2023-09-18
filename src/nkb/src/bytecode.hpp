#ifndef HEADER_GUARD_NKB_BYTECODE
#define HEADER_GUARD_NKB_BYTECODE

#include <cstddef>
#include <mutex>

#include "nk/common/allocator.h"
#include "nk/common/array.hpp"
#include "nk/common/hash_map.hpp"
#include "nk/common/id.h"
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
    size_t post_offset;
    nktype_t type;
    NkBcRefKind kind;
    uint8_t indir;
};

struct NkBcRefArray {
    NkBcRef *data;
    size_t size;
};

typedef enum {
    NkBcArg_None,

    NkBcArg_Ref,
    NkBcArg_RefArray,
} NkBcArgKind;

struct NkBcArg {
    union {
        NkBcRef ref;
        NkBcRefArray refs;
    };
    NkBcArgKind kind;
};

struct NkBcInstr {
    NkBcArg arg[3];
    uint16_t code;
};

typedef struct NkBcProc_T *NkBcProc;

struct NkBcProc_T {
    NkIrRunCtx ctx;
    size_t frame_size;
    NkArray<NkBcInstr> instrs;
};

struct NkFfiContext {
    NkAllocator alloc;
    NkHashMap<nktype_t, void *> typemap{};
    std::mutex mtx{};
};

struct NkIrRunCtx_T {
    NkIrProg ir;
    NkArena *tmp_arena;

    NkArray<NkBcProc> procs;
    NkArray<void *> globals;
    NkHashMap<nkid, void *> extern_syms;

    NkFfiContext ffi_ctx;
};

#endif // HEADER_GUARD_NKB_BYTECODE
