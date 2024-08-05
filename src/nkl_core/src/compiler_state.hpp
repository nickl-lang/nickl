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

struct NkAtomListNode {
    NkAtomListNode *next;
    NkAtom name;
};

struct DeferListNode {
    DeferListNode *next;
    NkIrInstrDynArray instrs;
    NkAtom file;
    u32 node_idx;
};

struct NodeListNode {
    NodeListNode *next;
    NklAstNode const &node;
};

enum LabelName {
    LabelName_Start,
    LabelName_Else,
    LabelName_Endif,
    LabelName_Short,
    LabelName_Join,
    LabelName_Loop,
    LabelName_Endloop,

    LabelName_Count,
};

struct ProcListNode {
    ProcListNode *next;
    NkIrProc proc;
    DeferListNode *defer_node;
    bool has_return_in_last_block;
    u32 label_counts[LabelName_Count];
};

struct Scope {
    Scope *next;

    NkArena *main_arena;
    NkArena *temp_arena;
    NkArenaFrame temp_frame;

    DeclMap locals;

    NkAtomListNode *export_list;
    DeferListNode *defer_stack;

    ProcListNode *cur_proc;
};

struct Context {
    NklState nkl;
    NklCompiler c;
    NklModule m;
    NkIrProg ir;

    NkIrProc top_level_proc;
    NklSource const &src;

    Scope *scope_stack;
    NodeListNode *node_stack;
    ProcListNode *proc_stack;
};

struct FileContext_kv {
    NkAtom key;
    Context *val;
};
NK_HASH_TREE_TYPEDEF(FileContextMap, FileContext_kv);
NK_HASH_TREE_PROTO(FileContextMap, FileContext_kv, NkAtom);

struct NklCompiler_T {
    NkIrProg ir;
    NkIrProc top_level_proc;
    NkIrProc entry_point;

    NkArena run_ctx_temp_arena;
    NkIrRunCtx run_ctx;

    NklState nkl;
    NkArena perm_arena;
    NkArena temp_arenas[2];
    FileContextMap files;
    NklErrorState errors;

    usize word_size;

#define CACHED_TYPE(NAME, EXPR)                     \
    nkltype_t NK_CAT(_cached_, NAME){};             \
    nkltype_t NAME() {                              \
        auto &cached = NK_CAT(_cached_, NAME);      \
        return cached ? cached : (cached = (EXPR)); \
    };

    CACHED_TYPE(any_t, nkl_get_any(nkl, word_size))
    CACHED_TYPE(bool_t, nkl_get_bool(nkl))
    CACHED_TYPE(str_t, nkl_get_slice(nkl, word_size, i8_t(), true))
    CACHED_TYPE(type_t, nkl_get_typeref(nkl, word_size))
    CACHED_TYPE(usize_t, nkl_get_int(nkl, word_size, false))
    CACHED_TYPE(void_t, nkl_get_void(nkl))

#define X(TYPE, VALUE_TYPE) CACHED_TYPE(NK_CAT(TYPE, _t), nkl_get_numeric(nkl, VALUE_TYPE))
    NKIR_NUMERIC_ITERATE(X)
#undef X

#undef CACHED_TYPE
};

NK_HASH_TREE_TYPEDEF(NkAtomSet, NkAtom);
NK_HASH_TREE_PROTO(NkAtomSet, NkAtom, NkAtom);

struct NklModule_T {
    NklCompiler c;
    NkIrModule mod;

    NkAtomSet export_set;
};

NkArena *getNextTempArena(NklCompiler c, NkArena *conflict);

FileContext_kv &getContextForFile(NklCompiler c, NkAtom file);

NkIrLabel createLabel(Context &ctx, LabelName name);

void emit(Context &ctx, NkIrInstr const &instr);
void emitDefers(Context &ctx, Scope const *upto);

NkIrRef asRef(Context &ctx, Interm const &val);

void pushPublicScope(Context &ctx);
void pushPrivateScope(Context &ctx);
void popScope(Context &ctx);

void defineComptimeUnresolved(Context &ctx, NkAtom name, NklAstNode const &node);
void defineLocal(Context &ctx, NkAtom name, NkIrLocalVar var);
void defineParam(Context &ctx, NkAtom name, usize idx);

Decl &resolve(Context &ctx, NkAtom name);

bool isValueKnown(Interm const &val);
nklval_t getValueFromInterm(Context &ctx, Interm const &val);
NkIrData getRodataFromInterm(Context &ctx, Interm const &val);

bool isModule(Interm const &val);
Scope *getModuleScope(Interm const &val);

nkltype_t getValueType(Context &ctx, Value const &val);

template <class TRet = Interm>
NK_PRINTF_LIKE(2)
static TRet error(Context &ctx, char const *fmt, ...) {
    auto last_node = ctx.node_stack;
    auto err_token = last_node ? &ctx.src.tokens.data[last_node->node.token_idx] : nullptr;

    va_list ap;
    va_start(ap, fmt);
    nkl_vreportError(ctx.src.file, err_token, fmt, ap);
    va_end(ap);

    return TRet{};
}

#endif // NKL_CORE_COMPILER_STATE_HPP_
