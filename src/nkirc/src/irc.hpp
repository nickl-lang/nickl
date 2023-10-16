#ifndef HEADER_GUARD_NKIRC_IRC
#define HEADER_GUARD_NKIRC_IRC

#include <cstdint>
#include <mutex>

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
    Decl_GlobalVar,
    Decl_ExternData,
    Decl_ExternProc,

    Decl_Type,
    Decl_Const
};

struct Decl {
    union {
        size_t arg_index;
        ProcRecord proc;
        NkIrLocalVar local;
        NkIrGlobalVar global;
        NkIrExternData extern_data;
        NkIrExternProc extern_proc;
        nktype_t type;
        NkIrConst cnst;
    } as;
    EDeclKind kind;
};

struct NkIrParserState {
    NkHashMap<nkid, Decl *> decls;
    nks error_msg{};
    bool ok{};
};

typedef struct NkIrCompiler_T {
    NkIrProg ir{};
    NkIrProc entry_point{NKIR_INVALID_IDX};
    NkArena *tmp_arena{};
    NkArena file_arena{};

    NkArena parse_arena{};
    NkIrParserState parser{};

    uint8_t usize = sizeof(void *);
    NkHashMap<nks, nktype_t> fpmap{};
    uint64_t next_id{1};
    std::mutex mtx{};
} * NkIrCompiler;

NkIrCompiler nkirc_create(NkArena *tmp_arena);
void nkirc_free(NkIrCompiler c);

int nkir_compile(NkIrCompiler c, nks in_file, NkIrCompilerConfig conf);
int nkir_run(NkIrCompiler c, nks in_file);

bool nkir_compileFile(NkIrCompiler c, nks base_file, nks in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_IRC
