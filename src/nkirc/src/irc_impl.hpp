#ifndef NKIRC_IRC_IMPL_HPP_
#define NKIRC_IRC_IMPL_HPP_

#include <mutex>

#include "irc.h"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/hash_map.hpp"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Decl;

struct ProcRecord {
    NkIrProc proc;
    NkHashMap<NkAtom, Decl *> locals;
    NkHashMap<NkAtom, NkIrLabel> labels;
};

enum EDeclKind {
    Decl_None,

    Decl_Arg,
    Decl_Proc,
    Decl_LocalVar,
    Decl_Data,
    Decl_ExternData,
    Decl_ExternProc,

    Decl_Type,
};

struct Decl {
    union {
        usize arg_index;
        ProcRecord proc;
        NkIrLocalVar local;
        NkIrData data;
        NkIrExternData extern_data;
        NkIrExternProc extern_proc;
        nktype_t type;
    } as;
    EDeclKind kind;
};

struct NkIrParserState {
    NkHashMap<NkAtom, Decl *> decls;
    NkString error_msg{};
    NklToken error_token{};
    bool ok{};
};

struct NkIrCompiler_T {
    NkArena *tmp_arena;
    NkIrcConfig conf;

    NkIrProg ir{};
    NkIrModule mod{};
    NkIrProc entry_point{NKIR_INVALID_IDX};
    NkArena file_arena{};

    NkArena parse_arena{};
    NkIrParserState parser{};

    NkHashMap<NkString, nktype_t> fpmap{};
    u32 next_id{1};
    std::mutex mtx{};
};

#ifdef __cplusplus
}
#endif

#endif // NKIRC_IRC_IMPL_HPP_
