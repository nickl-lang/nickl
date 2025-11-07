#include "nickl_impl.h"
#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree_array.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(compiler);

#define TRY(EXPR)         \
    do {                  \
        if (!(EXPR)) {    \
            return false; \
        }                 \
    } while (0)

typedef enum {
    Entity_None = 0,

    Entity_Incomplete = 0,

    Entity_ExternProc,
    Entity_Proc,

    EntityKind_Count,
} EntityKind;

typedef struct Scope Scope;

typedef struct {
    Scope *scope;
    NkIrInstrDynArray ir;
} ProcEntity;

typedef struct {
    union {
        ProcEntity proc;
    };
    NklType type;
    EntityKind kind;
    bool is_pub; // TODO: Separate or rename to Decl?
    bool is_comptime;
} Entity;

NK_HASH_TREE_ARRAY_DEFINE_KV(EntityMap, NkAtom, Entity, nk_atom_hash, nk_atom_equal);

// typedef struct {
//     EntityMap names;
// } Namespace;

struct Scope {
    Scope *next;
    usize refcount;

    EntityMap names;
};

static u32 nodeIdx(NklAstNodeArray nodes, NklAstNode const *node) {
    return node - nodes.data;
}

typedef struct {
    NklAstNodeArray nodes;
    u32 next_idx;
} AstNodeIterator;

static AstNodeIterator nodeIterate(NklAstNodeArray nodes, NklAstNode const *node) {
    return (AstNodeIterator){
        .nodes = nodes,
        .next_idx = nodeIdx(nodes, node) + 1,
    };
}

static NklAstNode const *nextNode(AstNodeIterator *it) {
    NklAstNode const *next_n = &it->nodes.data[it->next_idx];
    it->next_idx = nkl_ast_nextChild(it->nodes, it->next_idx);
    return next_n;
}

// typedef struct {
// } Interm;

typedef struct {
    NklType type;
    Scope *scope;
    bool is_comptime;
} AstNodeExt;

typedef NkSlice(AstNodeExt) AstNodeExtArray;

typedef struct {
    NkArena *scratch;

    NklState nkl;
    NklModule mod;

    NklType proc_t;

    NklSource src;
    AstNodeExtArray nodes_ext;

    Scope *scope_stack;
} CompileCtx;

static NklToken const *getToken(CompileCtx *ctx, NklAstNode const *node) {
    return &ctx->src.tokens.data[node->token_idx];
}

static void vreportError(CompileCtx *ctx, NklAstNode const *node, char const *fmt, va_list ap) {
    NklToken const *token = getToken(ctx, node);
    nickl_vreportError(
        ctx->mod->com->nkl,
        (NklSourceLocation){
            .file = nk_atom2s(ctx->src.file),
            token->lin,
            token->col,
            token->len,
        },
        fmt,
        ap);
}

static NK_PRINTF_LIKE(3) void reportError(CompileCtx *ctx, NklAstNode const *node, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreportError(ctx, node, fmt, ap);
    va_end(ap);
}

static NkString parseString(CompileCtx *ctx, NkArena *arena, NklAstNode const *node) {
    nk_assert(node->id == n_string || node->id == n_escaped_string);

    NklToken const *token = getToken(ctx, node);

    NkString const token_str = nkl_getTokenStr(token, ctx->src.text);
    NkString const str = (NkString){token_str.data + 1, token_str.size - 2};

    if (node->id == n_string) {
        char const *cstr = nk_tprintf(arena, NKS_FMT, NKS_ARG(str));
        return (NkString){cstr, str.size};
    } else {
        NkStringBuilder sb = {.alloc = nk_arena_getAllocator(arena)};
        nks_unescape(nksb_getStream(&sb), str);
        if (NKS_LAST(sb)) {
            nksb_appendNull(&sb);
        }

        return (NkString){sb.data, sb.size - 1};
    }
}

static NkAtom parseId(CompileCtx *ctx, NklAstNode const *node) {
    nk_assert(node->id == n_id);
    NklToken const *token = getToken(ctx, node);
    NkString const token_str = nkl_getTokenStr(token, ctx->src.text);
    return nk_s2atom(token_str);
}

static bool typecheck(CompileCtx *ctx, NklAstNode const *node);
static void compile(CompileCtx *ctx, NklAstNode const *node);

static bool typecheckComptimeConst(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    AstNodeExt *node_ext = &ctx->nodes_ext.data[node_idx];

    TRY(typecheck(ctx, node));

    if (!node_ext->is_comptime) {
        reportError(ctx, node, "comptime const expected");
        return false;
    }

    return true;
}

static NklAny compileComptimeConst(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    AstNodeExt const *node_ext = &ctx->nodes_ext.data[node_idx];
    nk_assert(node_ext->type && "typecheck failed to compute type");

    reportError(ctx, node, "TODO: compileComptimeConst is not finished");
    return (NklAny){0};
}

static bool parseType(CompileCtx *ctx, NklAstNode const *node, NklType *out_type) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    if (node->id == n_ptr) {
        NklAstNode const *const_or_target_t_n = nextNode(&it);
        bool is_const = const_or_target_t_n->id == n_const;

        NklAstNode const *target_t_n = is_const ? nextNode(&it) : const_or_target_t_n;

        NklType target_t;
        TRY(parseType(ctx, target_t_n, &target_t));

        *out_type = nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, target_t, is_const);
        return true;
    }

#define X(NAME, VALUE_TYPE)                                    \
    else if (node->id == NK_CAT(n_, NAME)) {                   \
        *out_type = nkl_type_getNumeric(ctx->nkl, VALUE_TYPE); \
        return true;                                           \
    }
    NKIR_NUMERIC_ITERATE(X)
#undef X

    reportError(ctx, node, "TODO: parseType is not finished");
    return false;
}

static bool parseProcInfo(CompileCtx *ctx, NklAstNode const *node, NklProcInfo *out_info) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    NklAstNode const *params_n = nextNode(&it);
    NklAstNode const *ret_t_n = nextNode(&it);

    NklTypeDynArray param_types = {.alloc = nk_arena_getAllocator(ctx->scratch)};

    bool is_variadic = false;

    AstNodeIterator params_it = nodeIterate(ctx->src.nodes, params_n);
    for (u32 i = 0; i < params_n->arity; i++) {
        NklAstNode const *param_n = nextNode(&params_it);

        if (param_n->id == n_ellipsis) {
            is_variadic = true;
            continue;
        }

        AstNodeIterator param_it = nodeIterate(ctx->src.nodes, param_n);

        NklAstNode const *name_or_type_n = nextNode(&param_it);
        NklAstNode const *type_n = (name_or_type_n->id == n_id) ? nextNode(&param_it) : name_or_type_n;

        NklType param_t;
        TRY(parseType(ctx, type_n, &param_t));

        nkda_append(&param_types, param_t);
    }

    NklType ret_t;
    TRY(parseType(ctx, ret_t_n, &ret_t));

    *out_info = (NklProcInfo){
        .param_types = {NKS_INIT_STRIDED(param_types)},
        .ret_t = ret_t,
        .flags = is_variadic ? NklProc_Variadic : 0,
    };
    return true;
}

static bool typecheck(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Typechecking node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt *node_ext = &ctx->nodes_ext.data[node_idx];

    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    switch (node->id) {
        case 0: {
            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
        }

        case n_string:
        case n_escaped_string: {
            NkString const str = parseString(ctx, &ctx->nkl->arena, node);

            NklType const i8_t = nkl_type_getNumeric(ctx->nkl, Int8);
            NklType const str_t = nkl_type_getArray(ctx->nkl, i8_t, str.size + 1);

            // TODO: Define string symbol during compilation phase?
            NkAtom const sym = nk_atom_unique((NkString){0});
            TRY(nickl_defineSymbol(
                ctx->mod,
                &(NkIrSymbol){
                    .data =
                        {
                            .type = &str_t->ir_type,
                            .relocs = {0},
                            .addr = (void *)str.data,
                            .flags = NkIrData_ReadOnly,
                        },
                    .name = sym,
                    .vis = NkIrVisibility_Local,
                    .kind = NkIrSymbol_Data,
                }));

            *node_ext = (AstNodeExt){
                .type = str_t,
                .is_comptime = true,
            };
            return true;
        }

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Entity const *found = EntityMap_find(&ctx->scope_stack->names, name);
            if (!found) {
                reportError(ctx, node, "undeclared identifier `%s`", nk_atom2cs(name));
                return false;
            }

            *node_ext = (AstNodeExt){
                .type = found->type,
                .is_comptime = found->is_comptime,
            };
            return true;
        }

        case n_int: {
            *node_ext = (AstNodeExt){
                .type = nkl_type_getNumeric(ctx->nkl, Int64),
                .is_comptime = true,
            };
            return true;
        }

        case n_nullptr: {
            *node_ext = (AstNodeExt){
                .type = nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, nkl_type_getVoid(ctx->nkl), false),
                .is_comptime = true,
            };
            return true;
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                NklAstNode const *child_n = nextNode(&it);

                TRY(typecheck(ctx, child_n));

                AstNodeExt const *child_n_ext = &ctx->nodes_ext.data[nodeIdx(ctx->src.nodes, child_n)];
                *node_ext = *child_n_ext;
            }
            return true;
        }

        case n_call: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *args_n = nextNode(&it);

            TRY(typecheck(ctx, lhs_n));

            AstNodeExt const *lhs_node_ext = &ctx->nodes_ext.data[nodeIdx(ctx->src.nodes, lhs_n)];
            nk_assert(lhs_node_ext->type && "typecheck failed to compute type");

            if (lhs_node_ext->type->tclass != NklType_Procedure) {
                reportError(ctx, lhs_n, "proc expected");
                return false;
            }

            AstNodeIterator args_it = nodeIterate(ctx->src.nodes, args_n);

            // TODO: Typecheck args against proc_t
            for (u32 i = 0; i < args_n->arity; i++) {
                NklAstNode const *arg_n = nextNode(&args_it);
                TRY(typecheck(ctx, arg_n));
            }

            *node_ext = (AstNodeExt){
                .type = lhs_node_ext->type->as.proc.ret_t,
                .is_comptime = true,
            };
            return true;
        }

        case n_def: {
            NklAstNode const *pub_or_name_n = nextNode(&it);
            bool const is_pub = pub_or_name_n->id == n_pub;

            NklAstNode const *name_n = is_pub ? nextNode(&it) : pub_or_name_n;

            NklAstNode const *value_n = nextNode(&it);

            TRY(typecheckComptimeConst(ctx, value_n));

            AstNodeExt const *value_node_ext = &ctx->nodes_ext.data[nodeIdx(ctx->src.nodes, value_n)];
            nk_assert(value_node_ext->type && "typecheck failed to compute type");

            EntityMap_insert(
                &ctx->scope_stack->names,
                parseId(ctx, name_n),
                (Entity){
                    .type = value_node_ext->type,
                    .kind = Entity_ExternProc,
                    .is_pub = false,
                    .is_comptime = true,
                });

            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
        }

        case n_extern: {
            NklAstNode const *opt_lib_n = nextNode(&it);

            NklAstNode const *name_n;

            NkAtom lib = 0;
            if (opt_lib_n->id == n_string || opt_lib_n->id == n_escaped_string) {
                NkArena *scratch = nk_arena_getScratch(NULL);
                NkString const lib_str = parseString(ctx, scratch, opt_lib_n);
                lib = nk_s2atom(lib_str);

                name_n = nextNode(&it);
            } else {
                name_n = opt_lib_n;
            }

            NkAtom const name = parseId(ctx, name_n);

            NklAstNode const *decl_n = nextNode(&it);

            if (decl_n->id == n_proc) {
                NklProcInfo proc_info;
                TRY(parseProcInfo(ctx, decl_n, &proc_info));

                NkIrTypeDynArray ir_param_types = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)};
                NK_ITERATE_STRIDED(NklType const *, it, proc_info.param_types) {
                    NklType type = *it;
                    nkda_append(&ir_param_types, nkl_type_getIrType(type));
                }

                NklType const proc_t = nkl_type_getProcedure(ctx->nkl, ctx->mod->com->word_size, proc_info);
                EntityMap_insert(
                    &ctx->scope_stack->names,
                    name,
                    (Entity){
                        .type = proc_t,
                        .kind = Entity_ExternProc,
                        .is_pub = false,
                        .is_comptime = false,
                    });

                TRY(nickl_defineSymbol(
                    ctx->mod,
                    &(NkIrSymbol){
                        .extrn =
                            {
                                .proc =
                                    {
                                        .param_types = {NKS_INIT(ir_param_types)},
                                        .ret_type = nkl_type_getIrType(proc_info.ret_t),
                                        .flags = (proc_info.flags & NklProc_Variadic) ? NkIrProc_Variadic : 0,
                                    },
                                .lib = lib,
                                .kind = NkIrExtern_Proc,
                            },
                        .name = name,
                        .kind = NkIrSymbol_Extern,
                    }));
            } else {
                reportError(ctx, node, "TODO: Only proc extern is implemented");
                return false;
            }

            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
        }

        case n_proc: {
            NklProcInfo proc_info;
            TRY(parseProcInfo(ctx, node, &proc_info));

            *node_ext = (AstNodeExt){
                .type = nkl_type_getProcedure(ctx->nkl, ctx->mod->com->word_size, proc_info),
                .is_comptime = true,
            };
            return true;
        }

        default: {
            reportError(ctx, node, "unknown AST node `%s`", nk_atom2cs(node->id));
            return false;
        }
    }

    nk_assert(!"unreachable");
    return false;
}

static void compile(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Compiling node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt const *node_ext = &ctx->nodes_ext.data[node_idx];
    nk_assert(node_ext->type && "typecheck failed to compute type");

    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    switch (node->id) {
        case 0: {
            break;
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                compile(ctx, nextNode(&it));
            }
            break;
        }

        default: {
            reportError(ctx, node, "TODO: unhandled AST node `%s`", nk_atom2cs(node->id));
            break;
        }
    }
}

static void pushScope(CompileCtx *ctx) {
    Scope *scope = nk_arena_allocT(ctx->scratch, Scope);
    *scope = (Scope){
        .names = {.alloc = nk_arena_getAllocator(ctx->scratch)},
    };
    nk_list_push(ctx->scope_stack, scope);
}

static bool compileProc(NklModule mod, NklSource const *src, NklType proc_t) {
    bool ok = true;

    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        CompileCtx ctx = {
            .scratch = scratch,

            .nkl = mod->com->nkl,
            .mod = mod,

            .proc_t = proc_t,

            .src = *src,
            .nodes_ext =
                {
                    .data = nk_arena_allocTn(scratch, AstNodeExt, src->nodes.size),
                    .size = src->nodes.size,
                },
        };
        NKS_ZERO(ctx.nodes_ext);

        pushScope(&ctx);

        NklAstNode const *root = &NKS_FIRST(src->nodes);
        ok = typecheck(&ctx, root);
        if (ok) {
            compile(&ctx, root);
        }
    }

    return ok;
}

bool nickl_compile(NklModule mod, NklSource const *src) {
    NK_LOG_TRC("%s", __func__);

    bool ok = true;
    NK_PROF_FUNC() {
        if (src->nodes.size) {
            NklCompiler com = mod->com;
            NklState nkl = com->nkl;
            ok = compileProc(
                mod,
                src,
                nkl_type_getProcedure(
                    nkl,
                    com->word_size,
                    (NklProcInfo){
                        .param_types = {0},
                        .ret_t = nkl_type_getVoid(nkl),
                        .flags = 0,
                    }));
        }
    }
    return ok;
}
