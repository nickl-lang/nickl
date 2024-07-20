#ifndef NKL_CORE_COMPILER_STATE_H_
#define NKL_CORE_COMPILER_STATE_H_

#include "nkb/ir.h"
#include "nkl/core/compiler.h"
#include "nkl/core/types.h"
#include "ntk/hash_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

enum DeclKind {
    DeclKind_Undefined,

    DeclKind_Comptime,
    DeclKind_ComptimeIncomplete,
    DeclKind_ComptimeUnresolved,
    DeclKind_ExternData,
    DeclKind_ExternProc,
    DeclKind_Local,
    DeclKind_Module,
    DeclKind_ModuleIncomplete,
    DeclKind_Param,
};

struct Context;
struct Scope;

struct Decl {
    union {
        struct {
            nklval_t val;
        } comptime;
        struct {
            Context *ctx;
            usize node_idx;
        } comptime_unresolved;
        struct {
            NkIrLocalVar var;
        } local;
        struct {
            Scope *scope;
            NkIrProc proc;
        } module;
        struct {
            usize idx;
        } param;
        struct {
            NkIrExternProc id;
        } extern_proc;
        struct {
            NkIrExternData id;
        } extern_data;
    } as;
    DeclKind kind;
};

enum ValueKind {
    ValueKind_Void,

    ValueKind_Const,
    ValueKind_Decl,
    ValueKind_Instr,
    ValueKind_Ref,
};

struct ValueInfo {
    union {
        struct {
            NkIrData id;
        } cnst;
        Decl *decl;
        NkIrInstr instr;
        NkIrRef ref;
    } as;
    nkltype_t type;
    ValueKind kind;
};

struct Decl_kv {
    NkAtom key;
    Decl val;
};
NK_HASH_TREE_TYPEDEF(DeclMap, Decl_kv);
NK_HASH_TREE_PROTO(DeclMap, Decl_kv, NkAtom);

struct Scope {
    Scope *next;

    NkArena *main_arena;
    NkArena *temp_arena;
    NkArenaFrame temp_frame;

    DeclMap locals;

    NkIrProc proc;
};

struct NodeListNode {
    NodeListNode *next;
    usize node_idx;
};

struct Context {
    NklModule m;
    NklSource const *src;
    Scope *scope_stack;
    NodeListNode *node_stack;
};

struct FileContext {
    NkIrProc proc;
    Context *ctx;
};

struct FileContext_kv {
    NkAtom key;
    FileContext val;
};
NK_HASH_TREE_TYPEDEF(CompilerFileMap, FileContext_kv);
NK_HASH_TREE_PROTO(CompilerFileMap, FileContext_kv, NkAtom);

struct NklCompiler_T {
    NkIrProg ir;
    NkIrProc entry_point;

    NklState nkl;
    NkArena perm_arena;
    NkArena temp_arenas[2];
    CompilerFileMap files;
    NklErrorState errors;

    usize word_size;
};

struct NklModule_T {
    NklCompiler c;
    NkIrModule mod;
};

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_COMPILER_STATE_H_
