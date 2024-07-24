#ifndef NKL_CORE_COMPILER_STATE_HPP_
#define NKL_CORE_COMPILER_STATE_HPP_

#include "nkb/ir.h"
#include "nkl/core/compiler.h"
#include "nkl/core/types.h"
#include "ntk/hash_tree.h"

enum ValueKind {
    ValueKind_Void,

    ValueKind_Rodata,
    ValueKind_Proc,
    ValueKind_Data,
    ValueKind_ExternData,
    ValueKind_ExternProc,
    ValueKind_Local,
    ValueKind_Arg,
};

struct Scope;

struct Value {
    union {
        struct {
            NkIrData id;
            Scope *opt_scope;
        } rodata;
        struct {
            NkIrProc id;
            Scope *opt_scope;
        } proc;
        struct {
            NkIrData id;
        } data;
        struct {
            NkIrExternData id;
        } extern_data;
        struct {
            NkIrExternProc id;
        } extern_proc;
        struct {
            NkIrLocalVar id;
        } local;
        struct {
            usize idx;
        } arg;
    } as;
    ValueKind kind;
};

enum DeclKind {
    DeclKind_Undefined,

    DeclKind_Unresolved,
    DeclKind_Incomplete,
    DeclKind_Complete,
};

struct Context;

struct Decl {
    union {
        struct {
            Context *ctx;
            NklAstNode const *node;
        } unresolved;
        Value val;
    } as;
    DeclKind kind;
};

enum IntermKind {
    IntermKind_Void,

    IntermKind_Instr,
    IntermKind_Ref,
    IntermKind_Val
};

struct Interm {
    union {
        NkIrInstr instr;
        NkIrRef ref;
        Value val;
    } as;
    nkltype_t type;
    IntermKind kind;
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

    NkIrProc cur_proc;
};

struct NodeListNode {
    NodeListNode *next;

    NklAstNode const &node;
    nkltype_t type;
};

struct Context {
    NkIrProc top_level_proc;
    NklModule m;
    NklSource const &src;
    Scope *scope_stack;
    NodeListNode *node_stack;
};

struct FileContext_kv {
    NkAtom key;
    Context *val;
};
NK_HASH_TREE_TYPEDEF(FileContextMap, FileContext_kv);
NK_HASH_TREE_PROTO(FileContextMap, FileContext_kv, NkAtom);

struct NklCompiler_T {
    NkIrProg ir;
    NkIrProc entry_point;

    NkArena run_ctx_temp_arena;
    NkIrRunCtx run_ctx;

    NklState nkl;
    NkArena perm_arena;
    NkArena temp_arenas[2];
    FileContextMap files;
    NklErrorState errors;

    usize word_size;
};

struct NklModule_T {
    NklCompiler c;
    NkIrModule mod;
};

NkArena *getNextTempArena(NklCompiler c, NkArena *conflict);

FileContext_kv &getContextForFile(NklCompiler c, NkAtom file);

NkIrRef asRef(Context &ctx, Interm const &val);

void pushPublicScope(Context &ctx, NkIrProc cur_proc);
void pushPrivateScope(Context &ctx, NkIrProc cur_proc);
void popScope(Context &ctx);

void defineComptimeUnresolved(Context &ctx, NkAtom name, NklAstNode const &node);
void defineLocal(Context &ctx, NkAtom name, NkIrLocalVar var);
void defineParam(Context &ctx, NkAtom name, usize idx);
void defineExternProc(Context &ctx, NkAtom name, NkIrExternProc id);
void defineExternData(Context &ctx, NkAtom name, NkIrExternData id);

Decl &resolve(Context &ctx, NkAtom name);

bool isValueKnown(Interm const &val);
nklval_t getValueFromInterm(NklCompiler c, Interm const &val);

bool isModule(Interm const &val);
Scope *getModuleScope(Interm const &val);

nkltype_t getValueType(NklCompiler c, Value const &val);

#endif // NKL_CORE_COMPILER_STATE_HPP_
