#ifndef HEADER_GUARD_NKIRC_IRC_IMPL
#define HEADER_GUARD_NKIRC_IRC_IMPL

#include <cstdint>
#include <mutex>

#include "irc.h"
#include "lexer.h"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/hash_map.hpp"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Decl;

struct ProcRecord {
    NkIrProc proc;
    NkHashMap<nkid, Decl *> locals;
    NkHashMap<nkid, NkIrLabel> labels;
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
        size_t arg_index;
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
    NkHashMap<nkid, Decl *> decls;
    nks error_msg{};
    NklToken error_token{};
    bool ok{};
};

struct NkIrCompiler_T {
    NkArena *tmp_arena;
    NkIrcConfig conf;

    NkIrProg ir{};
    NkIrProc entry_point{NKIR_INVALID_IDX};
    NkArena file_arena{};

    NkArena parse_arena{};
    NkIrParserState parser{};

    NkHashMap<nks, nktype_t> fpmap{};
    uint64_t next_id{1};
    std::mutex mtx{};
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_IRC_IMPL
