#include <stdlib.h>

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
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(compiler);

#define TRY(EXPR)      \
    do {               \
        if (!(EXPR)) { \
            return 0;  \
        }              \
    } while (0)

typedef enum {
    Entity_Incomplete = 0,

    Entity_ExternProc,
    Entity_Proc,
    Entity_Typeref,

    EntityKind_Count,
} EntityKind;

typedef struct Scope Scope;

typedef struct {
    NkAtom name;
    NklFieldDynArray params;
    NklType ret_t;
    u8 flags;
    NklAstNode const *body_n;
} ProcInfo;

typedef struct {
    ProcInfo info;
    NkIrInstrDynArray ir;
    Scope *scope;
} ProcEntity;

typedef struct {
    union {
        ProcEntity proc;
        NklType typeref;
    };
    NkAtom sym;
    NklType type;
    EntityKind kind;
} Entity;

typedef enum {
    Decl_None = 0, // TODO: Remove?

    Decl_Entity,
    Decl_Extern,
    Decl_LocalVar,
    Decl_Param,
} DeclKind;

typedef struct {
    union {
        Entity *entity;
        NkAtom sym;
    };
    NklType type;
    DeclKind kind;
    bool is_pub;
    bool is_comptime;
    bool is_lvalue;
    bool is_const;
} Decl;

NK_HASH_TREE_ARRAY_DEFINE_KV(DeclMap, NkAtom, Decl, nk_atom_hash, nk_atom_equal);

// typedef struct {
//     DeclMap names;
// } Namespace;

struct Scope {
    Scope *next;
    usize refcount;

    DeclMap names;
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

typedef struct {
    union {
        Entity *entity;
        NkAtom sym;
    };
    NklType type;
    // Scope *scope;
    bool is_comptime;
    bool is_lvalue;
    bool is_const;
} AstNodeExt;

typedef NkSlice(AstNodeExt) AstNodeExtArray;

typedef NkDynArray(Entity *) EntityPtrDynArray;

NK_HASH_TREE_ARRAY_DEFINE_KV(EntityMap, NkAtom, Entity *, nk_atom_hash, nk_atom_equal);

typedef enum {
    Id_Value,
    Id_LabelElse,
    Id_LabelEndif,
    Id_LabelLoop,
    Id_LabelEndloop,
    Id_LabelShort,
    Id_LabelJoin,

    IdKind_Count,
} IdKind;

char const *s_id_names[] = {
    "v",       // Id_Value
    "else",    // Id_LabelElse
    "endif",   // Id_LabelEndif
    "loop",    // Id_LabelLoop
    "endloop", // Id_LabelEndloop
    "short",   // Id_LabelShort
    "join",    // Id_LabelJoin
};

typedef struct {
    NkArena *scratch;

    NklState nkl;
    NklModule mod;

    NklType proc_t;
    NkIrInstrDynArray *instrs;

    NklSource src;
    AstNodeExtArray nodes_ext;

    Scope *scope_stack;

    EntityMap procs_to_compile;

    u32 id_counters[IdKind_Count];
} CompileCtx;

static NkAtom getNextId(CompileCtx *ctx, IdKind kind) {
    return nk_s2atom(nk_tsprintf(ctx->scratch, "%s%u", s_id_names[kind], ctx->id_counters[kind]++));
}

static NkAtom getNextLocalVar(CompileCtx *ctx, NkAtom name) {
    return nk_s2atom(
        nk_tsprintf(ctx->scratch, "%s%u_%s", s_id_names[Id_Value], ctx->id_counters[Id_Value]++, nk_atom2cs(name)));
}

static NklToken const *getToken(CompileCtx *ctx, NklAstNode const *node) {
    return &ctx->src.tokens.data[node->token_idx];
}

static NkString getTokenStr(CompileCtx *ctx, NklAstNode const *node) {
    NklToken const *token = getToken(ctx, node);
    return nkl_getTokenStr(token, ctx->src.text);
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

    NkString const token_str = getTokenStr(ctx, node);
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

        return (NkString){sb.data, sb.size};
    }
}

static NkAtom parseId(CompileCtx *ctx, NklAstNode const *node) {
    nk_assert(node->id == n_id);
    NkString const token_str = getTokenStr(ctx, node);
    return nk_s2atom(token_str);
}

static AstNodeExt *setNodeExt(CompileCtx *ctx, NklAstNode const *node, AstNodeExt const *x) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    AstNodeExt *nodex = &ctx->nodes_ext.data[node_idx];
    *nodex = *x;
    return nodex;
}

static AstNodeExt const *getNodeExt(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    AstNodeExt const *nodex = &ctx->nodes_ext.data[node_idx];
    nk_assert(nodex->type && "typecheck failed to compute type");
    return nodex;
}

typedef struct {
    NklType type;
    NklTypeClass tclass;
} TypecheckArgs;

static AstNodeExt const *typecheck(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args);

static AstNodeExt const *typecheckComptimeConst(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    AstNodeExt const *nodex;
    TRY(nodex = typecheck(ctx, node, args));

    if (!nodex->is_comptime) {
        reportError(ctx, node, "comptime const expected");
        return NULL;
    }

    return nodex;
}

static AstNodeExt const *typecheckLvalue(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    AstNodeExt const *nodex;
    TRY(nodex = typecheck(ctx, node, args));

    if (!nodex->is_lvalue) {
        reportError(ctx, node, "lvalue expected");
        return NULL;
    }

    return nodex;
}

static NklType parseType(CompileCtx *ctx, NklAstNode const *node);

static bool parseParamsList(
    CompileCtx *ctx,
    NklAstNode const *params_n,
    NklFieldDynArray *out_params,
    bool *allow_ellipsis) {
    AstNodeIterator params_it = nodeIterate(ctx->src.nodes, params_n);
    for (u32 i = 0; i < params_n->arity; i++) {
        NklAstNode const *param_n = nextNode(&params_it);

        if (allow_ellipsis && param_n->id == n_ellipsis) {
            *allow_ellipsis = true;
            continue;
        }

        AstNodeIterator param_it = nodeIterate(ctx->src.nodes, param_n);

        NklAstNode const *type_n = nextNode(&param_it);
        NklAstNode const *name_n = param_n->arity == 2 ? nextNode(&param_it) : NULL;

        NklType type;
        TRY(type = parseType(ctx, type_n));

        nkda_append(
            out_params,
            ((NklField){
                .name = name_n ? parseId(ctx, name_n) : 0,
                .type = type,
            }));
    }

    return true;
}

static bool parseProcInfo(CompileCtx *ctx, NkArena *arena, NklAstNode const *node, ProcInfo *out_info) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    NklAstNode const *name_or_params_n = nextNode(&it);
    NklAstNode const *params_n = name_or_params_n->id == n_id ? nextNode(&it) : name_or_params_n;
    NklAstNode const *ret_t_n = nextNode(&it);
    NklAstNode const *body_n = nextNode(&it);

    NkAtom const name = name_or_params_n->id == n_id ? parseId(ctx, name_or_params_n) : nk_atom_unique((NkString){0});

    bool is_variadic = false;
    NklFieldDynArray params = {.alloc = nk_arena_getAllocator(arena)};
    TRY(parseParamsList(ctx, params_n, &params, &is_variadic));

    NklType ret_t;
    TRY(ret_t = parseType(ctx, ret_t_n));

    *out_info = (ProcInfo){
        .name = name,
        .params = {NKS_INIT(params)},
        .ret_t = ret_t,
        .flags = is_variadic ? NklProc_Variadic : 0,
        .body_n = body_n,
    };
    return true;
}

static NklType parseType(CompileCtx *ctx, NklAstNode const *node) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    if (node->id == n_ptr) {
        NklAstNode const *const_or_target_t_n = nextNode(&it);
        bool is_const = const_or_target_t_n->id == n_const;

        NklAstNode const *target_t_n = is_const ? nextNode(&it) : const_or_target_t_n;

        NklType target_t;
        TRY(target_t = parseType(ctx, target_t_n));

        return nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, target_t, is_const);
    }

#define X(NAME, VALUE_TYPE)                                    \
    else if (node->id == NK_CAT(n_, NAME)) {                   \
        return NK_CAT(nickl_get_, NK_CAT(NAME, _t))(ctx->nkl); \
    }
    NKIR_NUMERIC_ITERATE(X)
#undef X

    else if (node->id == n_void) {
        return nickl_get_void_t(ctx->nkl);
    }

    else if (node->id == n_boolean) {
        return nickl_get_bool_t(ctx->nkl);
    }

    else if (node->id == n_proc) {
        ProcInfo proc_info;
        TRY(parseProcInfo(ctx, ctx->scratch, node, &proc_info));

        return nkl_type_getProcedure(
            ctx->nkl,
            ctx->mod->com->word_size,
            (NklProcInfo){
                .param_types = {NKS_INIT_STRIDED_FROM_FIELD(proc_info.params, type)},
                .ret_t = proc_info.ret_t,
                .flags = proc_info.flags,
            });
    }

    else if (node->id == n_id) {
        NkAtom const name = parseId(ctx, node);

        Decl const *decl = DeclMap_find(&ctx->scope_stack->names, name);
        if (!decl) {
            reportError(ctx, node, "undeclared identifier `%s`", nk_atom2cs(name));
            return NULL;
        }

        if (decl->kind != Decl_Entity || decl->entity->kind != Entity_Typeref) {
            reportError(ctx, node, "type expected");
            return NULL;
        }

        return decl->entity->typeref;
    }

    reportError(ctx, node, "TODO: parseType is not finished");
    return NULL;
}

static AstNodeExt *typecheckImpl(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Typechecking node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    switch (node->id) {
        case 0: {
            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_string:
        case n_escaped_string: {
            NkString const str = parseString(ctx, &ctx->nkl->arena, node);

            NklType const i8_t = nickl_get_i8_t(ctx->nkl);
            NklType const arr_t = nkl_type_getArray(ctx->nkl, i8_t, str.size + 1);
            NklType const str_t = nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, arr_t, true);

            NkAtom const sym = nk_atom_unique((NkString){0});
            TRY(nickl_defineSymbol(
                ctx->mod,
                &(NkIrSymbol){
                    .data =
                        {
                            .type = nkl_type_toIr(arr_t),
                            .relocs = {0},
                            .addr = (void *)str.data,
                            .flags = NkIrData_ReadOnly,
                        },
                    .name = sym,
                    .vis = NkIrVisibility_Local,
                    .kind = NkIrSymbol_Data,
                }));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .sym = sym,
                    .type = str_t,
                    .is_comptime = true,
                });
        }

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Decl const *decl = DeclMap_find(&ctx->scope_stack->names, name);
            if (!decl) {
                reportError(ctx, node, "undeclared identifier `%s`", nk_atom2cs(name));
                return NULL;
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .entity = decl->kind == Decl_Entity ? decl->entity : NULL,
                    .type = decl->type,
                    .is_comptime = decl->is_comptime,
                    .is_lvalue = true,
                    .is_const = decl->is_const,
                });
        }

        case n_int:
        case n_float: {
            NklType const type = (args->type && args->type->tclass == NklType_Numeric) ? args->type
                                 : node->id == n_int                                   ? nickl_get_i64_t(ctx->nkl)
                                                                                       : nickl_get_f64_t(ctx->nkl);
            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = type,
                    .is_comptime = true,
                });
        }

        case n_true_lit:
        case n_false_lit: {
            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_bool_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_nullptr: {
            NklType const type =
                (args->type && args->type->tclass == NklType_Pointer)
                    ? args->type
                    : nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, nickl_get_void_t(ctx->nkl), false);
            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = type,
                    .is_comptime = true,
                });
        }

        case n_list: {
            AstNodeExt nodex;
            if (node->arity) {
                for (u32 i = 0; i < node->arity; i++) {
                    NklAstNode const *child_n = nextNode(&it);

                    AstNodeExt const *child_nodex;
                    TRY(child_nodex = typecheck(ctx, child_n, &(TypecheckArgs){0}));

                    nodex = *child_nodex;
                }
            } else {
                nodex = (AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                };
            }
            return setNodeExt(ctx, node, &nodex);
        }

        case n_add:
        case n_sub:
        case n_mul:
        case n_div:
        case n_mod:
        case n_lsh:
        case n_rsh:
        case n_xor:
        case n_bitor:
        case n_bitand:
        case n_lt:
        case n_gt:
        case n_le:
        case n_ge:
        case n_eq:
        case n_ne: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *rhs_n = nextNode(&it);

            AstNodeExt const *lhs_nodex;
            AstNodeExt const *rhs_nodex;

            TRY(lhs_nodex = typecheck(ctx, lhs_n, &(TypecheckArgs){.tclass = NklType_Numeric}));
            TRY(rhs_nodex = typecheck(ctx, rhs_n, &(TypecheckArgs){.type = lhs_nodex->type}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = (node->id >= n_lt && node->id <= n_ne) ? nickl_get_bool_t(ctx->nkl) : lhs_nodex->type,
                    .is_comptime = lhs_nodex->is_comptime && rhs_nodex->is_comptime,
                });
        }

        case n_and:
        case n_or: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *rhs_n = nextNode(&it);

            AstNodeExt const *lhs_nodex;
            AstNodeExt const *rhs_nodex;

            NklType const bool_t = nickl_get_bool_t(ctx->nkl);

            TRY(lhs_nodex = typecheck(ctx, lhs_n, &(TypecheckArgs){.type = bool_t}));
            TRY(rhs_nodex = typecheck(ctx, rhs_n, &(TypecheckArgs){.type = bool_t}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = bool_t,
                    .is_comptime = lhs_nodex->is_comptime && rhs_nodex->is_comptime,
                });
        }

        case n_not: {
            NklAstNode const *arg_n = nextNode(&it);

            NklType const bool_t = nickl_get_bool_t(ctx->nkl);

            AstNodeExt const *arg_nodex;
            TRY(arg_nodex = typecheck(ctx, arg_n, &(TypecheckArgs){.type = bool_t}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = bool_t,
                    .is_comptime = arg_nodex->is_comptime,
                });
        }

        case n_cast: {
            NklAstNode const *type_n = nextNode(&it);
            NklAstNode const *val_n = nextNode(&it);

            NklType type;
            TRY(type = parseType(ctx, type_n));

            AstNodeExt const *val_nodex;
            TRY(val_nodex = typecheck(ctx, val_n, &(TypecheckArgs){0}));

            // TODO: Check cast compatibility

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = type,
                    .is_comptime = val_nodex->is_comptime,
                });
        }

        case n_addr: {
            NklAstNode const *arg_n = nextNode(&it);

            AstNodeExt const *arg;
            TRY(arg = typecheckLvalue(ctx, arg_n, &(TypecheckArgs){0}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    // TODO: Hardcoded mutable pointer
                    .type = nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, arg->type, false),
                    .is_comptime = arg->is_comptime,
                });
        }

        case n_deref: {
            NklAstNode const *arg_n = nextNode(&it);

            AstNodeExt const *arg;
            TRY(arg = typecheck(ctx, arg_n, &(TypecheckArgs){.tclass = NklType_Pointer}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = arg->type->as.ptr.target_t,
                    .is_comptime = arg->is_comptime,
                    .is_lvalue = true,
                    .is_const = false, // TODO: Detect const lvalues
                });
        }

        case n_member: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *name_n = nextNode(&it);

            AstNodeExt const *lhs;
            TRY(lhs = typecheck(ctx, lhs_n, &(TypecheckArgs){0}));

            NkAtom const name = parseId(ctx, name_n);

            nk_assert(lhs->type->tclass == NklType_Struct);

            // TODO: Boilerplate field search
            usize idx = -1u;
            NK_ITERATE(NklField const *, it, lhs->type->as.strct.fields) {
                if (it->name == name) {
                    idx = NK_INDEX(it, lhs->type->as.strct.fields);
                }
            }

            if (idx == -1u) {
                reportError(ctx, node, "undefined field `%s`", nk_atom2cs(name));
                return NULL;
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = lhs->type->as.strct.fields.data[idx].type,
                    .is_comptime = lhs->is_comptime,
                    .is_lvalue = lhs->is_lvalue,
                    .is_const = false, // TODO: Detect const lvalues
                });
        }

        case n_assign: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *rhs_n = nextNode(&it);

            AstNodeExt const *lhs_nodex;
            TRY(lhs_nodex = typecheckLvalue(ctx, lhs_n, &(TypecheckArgs){0}));

            TRY(typecheck(ctx, rhs_n, &(TypecheckArgs){.type = lhs_nodex->type}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = lhs_nodex->type,
                    .is_comptime = false,
                });
        }

        case n_call: {
            NklAstNode const *proc_n = nextNode(&it);
            NklAstNode const *args_n = nextNode(&it);

            AstNodeExt const *proc_nodex;
            TRY(proc_nodex = typecheck(ctx, proc_n, &(TypecheckArgs){.tclass = NklType_Procedure}));

            AstNodeIterator args_it = nodeIterate(ctx->src.nodes, args_n);

            bool const is_variadic = (proc_nodex->type->as.proc.flags & NklProc_Variadic);
            usize const param_count = proc_nodex->type->as.proc.param_types.size;

            if ((!is_variadic && args_n->arity != param_count) || (is_variadic && args_n->arity < param_count)) {
                reportError(
                    ctx,
                    args_n,
                    "expected%s %zu argument%s, got %u",
                    is_variadic ? " at least" : "",
                    param_count,
                    param_count == 1 ? "" : "s",
                    args_n->arity);
                return NULL;
            }

            for (u32 i = 0; i < args_n->arity; i++) {
                NklAstNode const *arg_n = nextNode(&args_it);
                NklType const arg_t = i < param_count ? proc_nodex->type->as.proc.param_types.data[i] : NULL;
                TRY(typecheck(ctx, arg_n, &(TypecheckArgs){.type = arg_t}));
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = proc_nodex->type->as.proc.ret_t,
                    .is_comptime = true,
                });
        }

        case n_def: {
            NklAstNode const *pub_or_name_n = nextNode(&it);
            bool const is_pub = pub_or_name_n->id == n_pub;

            NklAstNode const *name_n = is_pub ? nextNode(&it) : pub_or_name_n;
            NklAstNode const *value_n = nextNode(&it);

            AstNodeExt const *value_nodex;
            TRY(value_nodex = typecheckComptimeConst(ctx, value_n, &(TypecheckArgs){0}));

            nk_assert(value_nodex->entity && "TODO: Is is_comptime == entity?");
            DeclMap_insert(
                &ctx->scope_stack->names,
                parseId(ctx, name_n),
                (Decl){
                    .entity = value_nodex->entity,
                    .type = value_nodex->type,
                    .kind = Decl_Entity,
                    .is_pub = is_pub,
                    .is_comptime = true,
                });

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_extern: {
            NklAstNode const *lib_or_decl_n = nextNode(&it);

            NklAstNode const *decl_n;

            NkAtom lib = 0;
            if (lib_or_decl_n->id == n_string || lib_or_decl_n->id == n_escaped_string) {
                NkString const lib_str = parseString(ctx, ctx->scratch, lib_or_decl_n);
                lib = nk_s2atom(lib_str);

                decl_n = nextNode(&it);
            } else {
                decl_n = lib_or_decl_n;
            }

            if (decl_n->id == n_proc) {
                ProcInfo proc_info;
                TRY(parseProcInfo(ctx, ctx->scratch, decl_n, &proc_info));

                NkIrTypeDynArray ir_param_types = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)};
                NK_ITERATE(NklField const *, it, proc_info.params) {
                    nkda_append(&ir_param_types, nkl_type_toIr(it->type));
                }

                NklType const proc_t = nkl_type_getProcedure(
                    ctx->nkl,
                    ctx->mod->com->word_size,
                    (NklProcInfo){
                        .param_types = {NKS_INIT_STRIDED_FROM_FIELD(proc_info.params, type)},
                        .ret_t = proc_info.ret_t,
                        .flags = proc_info.flags,
                    });
                DeclMap_insert(
                    &ctx->scope_stack->names,
                    proc_info.name,
                    (Decl){
                        .sym = proc_info.name,
                        .type = proc_t,
                        .kind = Decl_Extern,
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
                                        .ret_type = nkl_type_toIr(proc_info.ret_t),
                                        .flags = (proc_info.flags & NklProc_Variadic) ? NkIrProc_Variadic : 0,
                                    },
                                .lib = lib,
                                .kind = NkIrExtern_Proc,
                            },
                        .name = proc_info.name,
                        .kind = NkIrSymbol_Extern,
                    }));
            } else {
                reportError(ctx, node, "TODO: Only proc extern is implemented");
                return NULL;
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_if: {
            NklAstNode const *cond_n = nextNode(&it);
            NklAstNode const *body_n = nextNode(&it);
            NklAstNode const *else_n = node->arity == 3 ? nextNode(&it) : NULL;

            TRY(typecheck(ctx, cond_n, &(TypecheckArgs){.type = nickl_get_bool_t(ctx->nkl)}));

            // TODO: Skip typechecking branches if cond is comptime

            TRY(typecheck(ctx, body_n, &(TypecheckArgs){0}));
            if (else_n) {
                TRY(typecheck(ctx, else_n, &(TypecheckArgs){0}));
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_proc: {
            ProcInfo proc_info;
            TRY(parseProcInfo(ctx, &ctx->nkl->arena, node, &proc_info));

            NklType const proc_t = nkl_type_getProcedure(
                ctx->nkl,
                ctx->mod->com->word_size,
                (NklProcInfo){
                    .param_types = {NKS_INIT_STRIDED_FROM_FIELD(proc_info.params, type)},
                    .ret_t = proc_info.ret_t,
                    .flags = proc_info.flags,
                });

            Entity *proc = nk_arena_allocT(&ctx->nkl->arena, Entity);
            *proc = (Entity){
                .proc =
                    {
                        .info = proc_info,
                        .ir = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)},
                        .scope = ctx->scope_stack,
                    },
                .sym = proc_info.name,
                .type = proc_t,
                .kind = Entity_Proc,
            };

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .entity = proc,
                    .type = proc_t,
                    .is_comptime = true,
                });
        }

        case n_return: {
            if (node->arity) {
                NklAstNode const *arg_n = nextNode(&it);

                TRY(typecheck(ctx, arg_n, &(TypecheckArgs){.type = ctx->proc_t->as.proc.ret_t}));
            }

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_struct: {
            NklAstNode const *fields_n = nextNode(&it);

            // TODO: Allow declaring incomplete type before typechecking params

            NklFieldDynArray fields = {.alloc = nk_arena_getAllocator(ctx->scratch)};
            TRY(parseParamsList(ctx, fields_n, &fields, NULL));

            NklType const struct_t = nkl_type_getStruct(ctx->nkl, (NklFieldStridedArray){NKS_INIT_STRIDED(fields)});

            NklType const typeref_t = nkl_type_getTyperef(ctx->nkl, ctx->mod->com->word_size);

            Entity *entity = nk_arena_allocT(&ctx->nkl->arena, Entity);
            *entity = (Entity){
                .typeref = struct_t,
                .type = typeref_t,
                .kind = Entity_Typeref,
            };

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .entity = entity,
                    .type = typeref_t,
                    .is_comptime = true,
                });
        }

        case n_var: {
            NklAstNode const *name_n = nextNode(&it);
            NklAstNode const *type_n = nextNode(&it);
            NklAstNode const *val_n = node->arity > 2 ? nextNode(&it) : NULL;

            nk_assert(type_n->id || val_n);

            NkAtom const name = parseId(ctx, name_n);

            NklType type = NULL;
            if (type_n->id) {
                TRY(type = parseType(ctx, type_n));
            }

            AstNodeExt const *val_nodex = NULL;
            if (val_n) {
                TRY(val_nodex = typecheck(ctx, val_n, &(TypecheckArgs){.type = type}));
            }

            if (!type) {
                type = val_nodex->type;
            }

            DeclMap_insert(
                &ctx->scope_stack->names,
                name,
                (Decl){
                    .sym = getNextLocalVar(ctx, name),
                    .type = type,
                    .kind = Decl_LocalVar,
                    .is_pub = false,
                    .is_comptime = false,
                    .is_lvalue = true,
                    .is_const = false, // TODO: Detect const lvalues
                });

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        case n_while: {
            NklAstNode const *cond_n = nextNode(&it);
            NklAstNode const *body_n = nextNode(&it);

            TRY(typecheck(ctx, cond_n, &(TypecheckArgs){.type = nickl_get_bool_t(ctx->nkl)}));

            // TODO: Skip typechecking branches if cond is comptime

            TRY(typecheck(ctx, body_n, &(TypecheckArgs){0}));

            return setNodeExt(
                ctx,
                node,
                &(AstNodeExt){
                    .type = nickl_get_void_t(ctx->nkl),
                    .is_comptime = true,
                });
        }

        default: {
            reportError(ctx, node, "unknown AST node `%s`", nk_atom2cs(node->id));
            return NULL;
        }
    }

    nk_assert(!"unreachable");
    return NULL;
}

static AstNodeExt const *typecheck(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    AstNodeExt *nodex;
    TRY(nodex = typecheckImpl(ctx, node, args));

    NklType const dst_t = args->type;
    NklType const src_t = nodex->type;

    if (dst_t && src_t != dst_t) {
        if (dst_t->tclass == NklType_Pointer && src_t->tclass == NklType_Pointer &&
            src_t->as.ptr.target_t->tclass == NklType_Array &&
            src_t->as.ptr.target_t->as.arr.elem_t == dst_t->as.ptr.target_t) {
            nodex->type = dst_t;
        } else {
            NkStringBuilder msg = {.alloc = nk_arena_getAllocator(ctx->scratch)};
            NkStream out = nksb_getStream(&msg);
            nk_printf(out, "cannot convert a value of type `");
            nkl_type_inspect(out, src_t);
            nk_printf(out, "` to `");
            nkl_type_inspect(out, dst_t);
            nk_printf(out, "`");
            reportError(ctx, node, NKS_FMT, NKS_ARG(msg));
            return NULL;
        }
    } else if (args->tclass && src_t->tclass != args->tclass) {
        reportError(ctx, node, "TODO: tclass mismatch");
        return NULL;
    }

    return nodex;
}

static void emit(CompileCtx *ctx, NkIrInstr instr) {
    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_print(log, "Emitting ");
        nkir_inspectInstr(log, instr);
    }

    nkda_append(ctx->instrs, instr);
}

typedef enum {
    Interm_Void = 0,

    Interm_Ref,
    Interm_Instr,
} IntermKind;

typedef struct NK_NODISCARD {
    union {
        NkIrInstr instr;
        NkIrRef ref;
    };
    NklType type;
    IntermKind kind;
    bool indir;
} Interm;

static Interm makeVoid(CompileCtx *ctx) {
    return (Interm){
        .type = nickl_get_void_t(ctx->nkl),
        .kind = Interm_Void,
    };
}

static Interm makeRef(NkIrRef ref) {
    return (Interm){
        .ref = ref,
        .type = (NklType)ref.type,
        .kind = Interm_Ref,
    };
}

static Interm makeRefIndir(NkIrRef ref, NklType type) {
    return (Interm){
        .ref = ref,
        .type = type,
        .kind = Interm_Ref,
        .indir = true,
    };
}

static Interm makeInstr(NkIrInstr instr, NklType type) {
    return (Interm){
        .instr = instr,
        .type = type,
        .kind = Interm_Instr,
    };
}

static Interm makeInstrIndir(NkIrInstr instr, NklType type) {
    return (Interm){
        .instr = instr,
        .type = type,
        .kind = Interm_Instr,
        .indir = true,
    };
}

static void discard(CompileCtx *ctx, Interm interm) {
    switch (interm.kind) {
        case Interm_Void:
        case Interm_Ref:
            return;

        case Interm_Instr:
            emit(ctx, interm.instr);
            return;
    };

    nk_assert(!"unreachable");
}

static NkIrRef toRefDirect(CompileCtx *ctx, Interm interm) {
    switch (interm.kind) {
        case Interm_Void:
            return (NkIrRef){0};

        case Interm_Ref:
            return interm.ref;

        case Interm_Instr: {
            NkIrRef *dst = &interm.instr.arg[0].ref;
            if ((dst->kind == NkIrRef_None || dst->kind == NkIrRef_Null) && interm.type->size) {
                *dst = nkir_makeRefLocal(
                    getNextId(ctx, Id_Value), interm.indir ? ctx->mod->com->ptr_t : nkl_type_toIr(interm.type));
            }
            emit(ctx, interm.instr);
            return *dst;
        }
    };

    nk_assert(!"unreachable");
    return (NkIrRef){0};
}

static NkIrRef toRefDirectTyped(CompileCtx *ctx, Interm interm) {
    NkIrRef ref = toRefDirect(ctx, interm);
    ref.type = nkl_type_toIr(interm.type);
    return ref;
}

static NkIrRef toRef(CompileCtx *ctx, Interm interm) {
    NkIrRef const ref = toRefDirect(ctx, interm);
    if (interm.indir) {
        return toRef(ctx, makeInstr(nkir_make_load((NkIrRef){0}, ref), interm.type));
    } else {
        return ref;
    }
}

static Interm resolveDecl(CompileCtx *ctx, Decl const *decl) {
    switch (decl->kind) {
        case Decl_None:
            nk_assert(!"unreachable");
            return (Interm){0};

        case Decl_Entity: {
            EntityMap_insert(&ctx->procs_to_compile, decl->sym, decl->entity);
            return makeRef(nkir_makeRefGlobal(decl->entity->sym, nkl_type_toIr(decl->type)));
        }

        case Decl_Extern:
            return makeRef(nkir_makeRefGlobal(decl->sym, nkl_type_toIr(decl->type)));

        case Decl_LocalVar:
            return makeRefIndir(nkir_makeRefLocal(decl->sym, ctx->mod->com->ptr_t), decl->type);

        case Decl_Param: {
            if (nkl_type_toIr(decl->type)->kind == NkIrType_Aggregate) {
                return makeRefIndir(nkir_makeRefParam(decl->sym, ctx->mod->com->ptr_t), decl->type);
            } else {
                return makeRef(nkir_makeRefParam(decl->sym, nkl_type_toIr(decl->type)));
            }
        }
    }

    nk_assert(!"unreachable");
    return (Interm){0};
}

static Interm compile(CompileCtx *ctx, NklAstNode const *node);

static void parseNumber(CompileCtx *ctx, void *addr, NkString str, NkIrNumericValueType value_type) {
    char const *cstr = nk_tprintf(ctx->scratch, NKS_FMT, NKS_ARG(str));

    char *endptr = NULL;

    switch (value_type) {
        case Int8:
            *(i8 *)addr = strtol(cstr, &endptr, 0);
            break;
        case Uint8:
            *(u8 *)addr = strtoul(cstr, &endptr, 0);
            break;
        case Int16:
            *(i16 *)addr = strtol(cstr, &endptr, 0);
            break;
        case Uint16:
            *(u16 *)addr = strtoul(cstr, &endptr, 0);
            break;
        case Int32:
            *(i32 *)addr = strtol(cstr, &endptr, 0);
            break;
        case Uint32:
            *(u32 *)addr = strtoul(cstr, &endptr, 0);
            break;
        case Int64:
            *(i64 *)addr = strtoll(cstr, &endptr, 0);
            break;
        case Uint64:
            *(u64 *)addr = strtoull(cstr, &endptr, 0);
            break;
        case Float32: {
            if (nks_startsWith(str, nk_cs2s("0x"))) {
                union {
                    f32 f;
                    u32 i;
                } pun = {.i = strtoul(cstr, &endptr, 0)};
                *(f32 *)addr = pun.f;
            } else {
                *(f32 *)addr = strtof(cstr, &endptr);
            }
            break;
        }
        case Float64:
            if (nks_startsWith(str, nk_cs2s("0x"))) {
                union {
                    f64 f;
                    u64 i;
                } pun = {.i = strtoull(cstr, &endptr, 0)};
                *(f64 *)addr = pun.f;
            } else {
                *(f64 *)addr = strtod(cstr, &endptr);
            }
            break;
    }

    nk_assert(endptr == cstr + str.size && "failed to parse numeric constant");
}

// static Interm compileRegisterStore(CompileCtx *ctx, Interm dst, Interm src) {
//     NklType const dst_t = dst.type;
//     NklType const src_t = dst.type;
//     if (src_t->size) {
//         if (src.kind == Interm_Instr &&
//             (src.instr.arg[0].ref.kind == NkIrRef_None || src.instr.arg[0].ref.kind == NkIrRef_Null)) {
//             src.instr.arg[0].ref = toRef(ctx, dst);
//         } else {
//             src = makeInstr(nkir_make_mov(toRef(ctx, dst), toRef(ctx, src)), dst_t);
//         }
//     }
//     // TODO: Can we defer materializing this interm? (most likely yes)
//     return makeRef(toRef(ctx, src));
// }

static Interm compileMemoryStore(CompileCtx *ctx, Interm dst, Interm src) {
    nk_assert(dst.indir);

    NklType const dst_t = dst.type;
    NklType const src_t = dst.type;

    if (src_t->size) {
        return makeInstr(nkir_make_store(toRefDirect(ctx, dst), toRef(ctx, src)), dst_t);
    } else {
        return src;
    }
}

static NklAny compileComptimeConst(CompileCtx *ctx, NklAstNode const *node) {
    reportError(ctx, node, "TODO: compileComptimeConst is not implemented");
    return (NklAny){0};
}

static NklType promote(CompileCtx *ctx, NklType type) {
    if (type->tclass == NklType_Numeric)
        switch (type->as.num.value_type) {
            case Int8:
            case Int16:
                type = nickl_get_i32_t(ctx->nkl);
                break;

            case Uint8:
            case Uint16:
                type = nickl_get_u32_t(ctx->nkl);
                break;

            case Float32:
                type = nickl_get_f64_t(ctx->nkl);
                break;

            default:
                break;
        }

    return type;
}

static void vcomment(CompileCtx *ctx, char const *fmt, va_list ap) {
    emit(ctx, nkir_make_comment(nk_vtsprintf(&ctx->nkl->arena, fmt, ap)));
}

static NK_PRINTF_LIKE(2) void comment(CompileCtx *ctx, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vcomment(ctx, fmt, ap);
    va_end(ap);
}

static Interm compileLogic(CompileCtx *ctx, NklAstNode const *node, bool invert);

static Interm compileLogicExpr(
    CompileCtx *ctx,
    NkAtom op,
    NklAstNode const *lhs_n,
    NklAstNode const *rhs_n,
    bool invert) {
    nk_assert(op == n_and || op == n_or);

#ifdef ENABLE_LOGGING
    comment(ctx, ">>>>>>> %s (node %u)", nk_atom2cs(op), nodeIdx(ctx->src.nodes, lhs_n) - 1);
#endif // ENABLE_LOGGING

    NkIrLabel const short_l = nkir_makeLabelAbs(getNextId(ctx, Id_LabelShort));
    NkIrLabel const join_l = nkir_makeLabelAbs(getNextId(ctx, Id_LabelJoin));

    NklType const bool_t = nickl_get_bool_t(ctx->nkl);
    Interm const res = makeRefIndir(nkir_makeRefLocal(getNextId(ctx, Id_Value), ctx->mod->com->ptr_t), bool_t);

    emit(ctx, nkir_make_alloc(res.ref, nkl_type_toIr(bool_t)));
    Interm const lhs = makeRef(toRef(ctx, compileLogic(ctx, lhs_n, invert)));
    if (op == (invert ? n_or : n_and)) {
        emit(ctx, nkir_make_jmpz(toRef(ctx, lhs), short_l));
    } else {
        emit(ctx, nkir_make_jmpnz(toRef(ctx, lhs), short_l));
    }
    Interm const rhs = compileLogic(ctx, rhs_n, invert);
    discard(ctx, compileMemoryStore(ctx, res, rhs));
    emit(ctx, nkir_make_jmp(join_l));

    emit(ctx, nkir_make_label(short_l.name));
    discard(ctx, compileMemoryStore(ctx, res, lhs));
    emit(ctx, nkir_make_jmp(join_l));

    emit(ctx, nkir_make_label(join_l.name));

#ifdef ENABLE_LOGGING
    comment(ctx, "<<<<<<< %s (node %u)", nk_atom2cs(op), nodeIdx(ctx->src.nodes, lhs_n) - 1);
#endif // ENABLE_LOGGING

    return res;
}

static Interm compileLogic(CompileCtx *ctx, NklAstNode const *node, bool invert) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    NklAstNode const *lhs_n = nextNode(&it);
    NklAstNode const *rhs_n = nextNode(&it);

    switch (node->id) {
        case n_and:
            return compileLogicExpr(ctx, n_and, lhs_n, rhs_n, invert);

        case n_or:
            return compileLogicExpr(ctx, n_or, lhs_n, rhs_n, invert);

        case n_not:
            return compileLogic(ctx, lhs_n, !invert);

        default: {
            Interm const res = compile(ctx, node);
            if (invert) {
                NklType const bool_t = nickl_get_bool_t(ctx->nkl);
                return makeInstr(
                    nkir_make_xor(
                        (NkIrRef){0}, nkir_makeRefImm((NkIrImm){.u8 = 1}, nkl_type_toIr(bool_t)), toRef(ctx, res)),
                    bool_t);
            } else {
                return res;
            }
        }
    }

    nk_assert(!"unreachable");
    return (Interm){0};
}

static Interm cast(CompileCtx *ctx, NklType dst_t, Interm val) {
    if (val.type != dst_t) {
        return makeInstr(nkir_make_cast((NkIrRef){0}, toRef(ctx, val)), dst_t);
    } else {
        return val;
    }
}

static Interm compile(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Compiling node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt const *nodex = getNodeExt(ctx, node);

    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    switch (node->id) {
        case n_extern:
        case n_def:
        case 0:
            return makeVoid(ctx);

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Decl const *decl = DeclMap_find(&ctx->scope_stack->names, name);
            nk_assert(decl);

            return resolveDecl(ctx, decl);
        }

        case n_int:
        case n_float: {
            nk_assert(nodex->type->tclass == NklType_Numeric);

            NkString const token_str = getTokenStr(ctx, node);

            NkIrImm imm = {0};
            parseNumber(ctx, &imm, token_str, nodex->type->as.num.value_type);
            return makeRef(nkir_makeRefImm(imm, nkl_type_toIr(nodex->type)));
        }

        case n_string:
        case n_escaped_string:
            return makeRef(nkir_makeRefGlobal(nodex->sym, nkl_type_toIr(nodex->type)));

        case n_true_lit:
            return makeRef(nkir_makeRefImm((NkIrImm){.u8 = 1}, nkl_type_toIr(nodex->type)));

        case n_false_lit:
            return makeRef(nkir_makeRefImm((NkIrImm){.u8 = 0}, nkl_type_toIr(nodex->type)));

        case n_nullptr:
            return makeRef(nkir_makeRefImm((NkIrImm){.u64 = 0}, nkl_type_toIr(nodex->type)));

        case n_list: {
            Interm res = {0};
            for (u32 i = 0; i < node->arity; i++) {
                discard(ctx, res);
                res = compile(ctx, nextNode(&it));
            }
            return res;
        }

#define BINOP(ID, NAME)                                                                                          \
    case NK_CAT(n_, ID): {                                                                                       \
        NklAstNode const *lhs_n = nextNode(&it);                                                                 \
        NklAstNode const *rhs_n = nextNode(&it);                                                                 \
                                                                                                                 \
        Interm const lhs = compile(ctx, lhs_n);                                                                  \
        Interm const rhs = compile(ctx, rhs_n);                                                                  \
                                                                                                                 \
        return makeInstr(NK_CAT(nkir_make_, NAME)((NkIrRef){0}, toRef(ctx, lhs), toRef(ctx, rhs)), nodex->type); \
    }

            BINOP(add, add)
            BINOP(sub, sub)
            BINOP(mul, mul)
            BINOP(div, div)
            BINOP(mod, mod)

            BINOP(lsh, lsh)
            BINOP(rsh, rsh)
            BINOP(xor, xor)
            BINOP(bitor, or)
            BINOP(bitand, and)

            BINOP(lt, cmp_lt)
            BINOP(gt, cmp_gt)
            BINOP(le, cmp_le)
            BINOP(ge, cmp_ge)
            BINOP(eq, cmp_eq)
            BINOP(ne, cmp_ne)

#undef BINOP

        case n_and:
        case n_or:
        case n_not:
            return compileLogic(ctx, node, false);

        case n_cast: {
            nextNode(&it); // type
            NklAstNode const *val_n = nextNode(&it);
            Interm const val = compile(ctx, val_n);
            return cast(ctx, nodex->type, val);
        }

        case n_addr: {
            NklAstNode const *arg_n = nextNode(&it);
            Interm const addr = compile(ctx, arg_n);
            return makeRef(toRefDirect(ctx, addr));
        }

        case n_deref: {
            NklAstNode const *arg_n = nextNode(&it);
            Interm const arg = compile(ctx, arg_n);
            return makeRefIndir(toRef(ctx, arg), nodex->type);
        }

        case n_member: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *name_n = nextNode(&it);

            Interm const lhs = compile(ctx, lhs_n);
            NkAtom const name = parseId(ctx, name_n);

            nk_assert(lhs.indir);
            nk_assert(lhs.type->tclass == NklType_Struct);

            // TODO: Hardcoded i32 for offset calc
            NklType const i32_t = nickl_get_i32_t(ctx->nkl);

            // TODO: Boilerplate field search
            usize idx = -1u;
            NK_ITERATE(NklField const *, it, lhs.type->as.strct.fields) {
                if (it->name == name) {
                    idx = NK_INDEX(it, lhs.type->as.strct.fields);
                }
            }
            nk_assert(idx < -1u);

            return makeInstrIndir(
                nkir_make_offset(
                    (NkIrRef){0},
                    toRefDirectTyped(ctx, lhs),
                    nkir_makeRefImm((NkIrImm){.i32 = idx}, nkl_type_toIr(i32_t))),
                nodex->type);
        }

        case n_assign: {
            NklAstNode const *lhs_n = nextNode(&it);
            NklAstNode const *rhs_n = nextNode(&it);

            Interm const lhs = compile(ctx, lhs_n);
            Interm const rhs = compile(ctx, rhs_n);

            return compileMemoryStore(ctx, lhs, rhs);
        }

        case n_call: {
            NklAstNode const *proc_n = nextNode(&it);
            NklAstNode const *args_n = nextNode(&it);

            Interm const proc = compile(ctx, proc_n);

            AstNodeIterator args_it = nodeIterate(ctx->src.nodes, args_n);

            NkIrRefDynArray args = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)};

            for (u32 i = 0; i < args_n->arity; i++) {
                if (i == proc.type->as.proc.param_types.size) {
                    nkda_append(&args, nkir_makeVariadicMarker());
                }

                NklAstNode const *arg_n = nextNode(&args_it);
                Interm arg = compile(ctx, arg_n);

                if (i >= proc.type->as.proc.param_types.size) {
                    arg = cast(ctx, promote(ctx, arg.type), arg);
                }

                if (nkl_type_toIr(arg.type)->kind == NkIrType_Aggregate) {
                    nk_assert(arg.indir);
                    nkda_append(&args, toRefDirectTyped(ctx, arg));
                } else {
                    nkda_append(&args, toRef(ctx, arg));
                }
            }

            return makeInstr(
                nkir_make_call(
                    nkir_makeRefNull(nkl_type_toIr(nodex->type)), toRef(ctx, proc), (NkIrRefArray){NKS_INIT(args)}),
                proc.type->as.proc.ret_t);
        }

        case n_if: {
#ifdef ENABLE_LOGGING
            comment(ctx, ">>>>>>> if (node %u)", node_idx);
#endif // ENABLE_LOGGING

            NklAstNode const *cond_n = nextNode(&it);
            NklAstNode const *body_n = nextNode(&it);
            NklAstNode const *else_n = node->arity == 3 ? nextNode(&it) : NULL;

            NkIrLabel const endif_l = nkir_makeLabelAbs(getNextId(ctx, Id_LabelEndif));
            NkIrLabel const else_l = else_n ? nkir_makeLabelAbs(getNextId(ctx, Id_LabelElse)) : endif_l;

            Interm const cond = compile(ctx, cond_n);

            emit(ctx, nkir_make_jmpz(toRef(ctx, cond), else_l));

            discard(ctx, compile(ctx, body_n));

            if (else_n) {
                emit(ctx, nkir_make_jmp(endif_l));
                emit(ctx, nkir_make_label(else_l.name));

                discard(ctx, compile(ctx, else_n));
            }

            emit(ctx, nkir_make_jmp(endif_l));
            emit(ctx, nkir_make_label(endif_l.name));

#ifdef ENABLE_LOGGING
            comment(ctx, "<<<<<<< if (node %u)", node_idx);
#endif // ENABLE_LOGGING

            return makeVoid(ctx);
        }

        case n_proc: {
            EntityMap_insert(&ctx->procs_to_compile, nodex->entity->sym, nodex->entity);
            return makeRef(nkir_makeRefGlobal(nodex->entity->sym, nkl_type_toIr(nodex->type)));
        }

        case n_return: {
            NkIrRef arg_ref = {0};
            if (node->arity) {
                NklAstNode const *arg_n = nextNode(&it);

                Interm const arg = compile(ctx, arg_n);
                arg_ref = toRef(ctx, arg);
            }

            return makeInstr(nkir_make_ret(arg_ref), nickl_get_void_t(ctx->nkl));
        }

        case n_var: {
            NklAstNode const *name_n = nextNode(&it);
            nextNode(&it); // type
            NklAstNode const *val_n = node->arity > 2 ? nextNode(&it) : NULL;

            NkAtom const name = parseId(ctx, name_n);

            Decl const *decl = DeclMap_find(&ctx->scope_stack->names, name);
            nk_assert(decl);

            Interm const var = makeRefIndir(nkir_makeRefLocal(decl->sym, ctx->mod->com->ptr_t), decl->type);

            emit(ctx, nkir_make_alloc(var.ref, nkl_type_toIr(decl->type)));

            if (val_n) {
                Interm const val = compile(ctx, val_n);
                discard(ctx, compileMemoryStore(ctx, var, val));
            }
            // TODO: Zero init

            return makeVoid(ctx);
        }

        case n_while: {
#ifdef ENABLE_LOGGING
            comment(ctx, ">>>>>>> while (node %u)", node_idx);
#endif // ENABLE_LOGGING

            NklAstNode const *cond_n = nextNode(&it);
            NklAstNode const *body_n = nextNode(&it);

            NkIrLabel const loop_l = nkir_makeLabelAbs(getNextId(ctx, Id_LabelLoop));
            NkIrLabel const endloop_l = nkir_makeLabelAbs(getNextId(ctx, Id_LabelEndloop));

            emit(ctx, nkir_make_jmp(loop_l));
            emit(ctx, nkir_make_label(loop_l.name));

            Interm const cond = compile(ctx, cond_n);

            emit(ctx, nkir_make_jmpz(toRef(ctx, cond), endloop_l));

            discard(ctx, compile(ctx, body_n));

            emit(ctx, nkir_make_jmp(loop_l));
            emit(ctx, nkir_make_label(endloop_l.name));

#ifdef ENABLE_LOGGING
            comment(ctx, "<<<<<<< while (node %u)", node_idx);
#endif // ENABLE_LOGGING

            return makeVoid(ctx);
        }

        default: {
            reportError(ctx, node, "TODO: unhandled AST node `%s`", nk_atom2cs(node->id));
            return (Interm){0};
        }
    }

    nk_assert(!"unreachable");
    return (Interm){0};
}

static void pushScope(CompileCtx *ctx) {
    Scope *scope = nk_arena_allocT(ctx->scratch, Scope);
    *scope = (Scope){
        .names = {.alloc = nk_arena_getAllocator(ctx->scratch)},
    };
    nk_list_push(ctx->scope_stack, scope);
}

static bool compileProc(NklModule mod, NklSource const *src, Entity *proc_e);

static bool compileProcImpl(CompileCtx *ctx, Entity *proc_e) {
    NkIrParamDynArray ir_params = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)};

    NK_ITERATE(NklField const *, it, proc_e->proc.info.params) {
        DeclMap_insert(
            &ctx->scope_stack->names,
            it->name,
            (Decl){
                .sym = it->name,
                .type = it->type,
                .kind = Decl_Param,
            });

        nkda_append(
            &ir_params,
            ((NkIrParam){
                .name = it->name,
                .type = nkl_type_toIr(it->type),
            }));
    }

    TRY(typecheck(ctx, proc_e->proc.info.body_n, &(TypecheckArgs){0}));
    discard(ctx, compile(ctx, proc_e->proc.info.body_n));

    if (!proc_e->proc.ir.size || NKS_LAST(proc_e->proc.ir).code != NkIrOp_ret) {
        if (proc_e->type->as.proc.ret_t->tclass != NklType_Void) {
            // TODO: Point to a better node
            reportError(ctx, proc_e->proc.info.body_n, "missing return statement");
            return false;
        }
        emit(ctx, nkir_make_ret((NkIrRef){0}));
    }

    while (ctx->procs_to_compile.size) {
        Entity *dep = NKS_LAST(ctx->procs_to_compile).val;
        nkda_pop(&ctx->procs_to_compile, 1);

        TRY(compileProc(ctx->mod, &ctx->src, dep));
    }

    TRY(nickl_defineSymbol(
        ctx->mod,
        &(NkIrSymbol){
            .proc =
                {
                    .params = {NKS_INIT(ir_params)},
                    .ret =
                        {
                            .type = nkl_type_toIr(proc_e->type->as.proc.ret_t),
                        },
                    .instrs = {NKS_INIT(proc_e->proc.ir)},
                    .flags = 0,
                },
            .name = proc_e->sym,
            .vis = NkIrVisibility_Hidden, // TODO: Prevent symbol being optimized away
            .kind = NkIrSymbol_Proc,
        }));

    return true;
}

static bool compileProc(NklModule mod, NklSource const *src, Entity *proc_e) {
    bool ok = true;

    nk_assert(proc_e->type->tclass == NklType_Procedure);

    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        CompileCtx ctx = {
            .scratch = scratch,

            .nkl = mod->com->nkl,
            .mod = mod,

            .proc_t = proc_e->type,
            .instrs = &proc_e->proc.ir,

            .src = *src,
            .nodes_ext =
                {
                    .data = nk_arena_allocTn(scratch, AstNodeExt, src->nodes.size),
                    .size = src->nodes.size,
                },

            .scope_stack = proc_e->proc.scope,

            .procs_to_compile = {.alloc = nk_arena_getAllocator(scratch)},
        };
        NKS_ZERO(ctx.nodes_ext);

        ok = compileProcImpl(&ctx, proc_e);
    }

    return ok;
}

bool nickl_compile(NklModule mod, NklSource const *src) {
    NK_LOG_TRC("%s", __func__);

    nickl_reportErrorLoc(
        mod->com->nkl,
        (NklSourceLocation){
            .file = nk_atom2s(src->file),
        },
        "TODO: nickl_compile is not implemented");
    return false;
}

bool nickl_TMP_compileAndRunFile(NklModule mod, NklSource const *src) {
    NK_LOG_TRC("%s", __func__);

    bool ok = true;
    NK_PROF_FUNC() {
        if (src->nodes.size) {
            NklCompiler com = mod->com;
            NklState nkl = com->nkl;

            NklType const proc_t = nkl_type_getProcedure(
                nkl,
                com->word_size,
                (NklProcInfo){
                    .param_types = {0},
                    .ret_t = nickl_get_void_t(nkl),
                    .flags = 0,
                });

            Scope scope = {
                .names = {.alloc = nk_arena_getAllocator(&nkl->arena)},
            };

            NkAtom const sym = nk_atom_unique((NkString){0});

            Entity proc_e = (Entity){
                .proc =
                    {
                        .info =
                            {
                                .name = sym,
                                .params = {0},
                                .ret_t = nickl_get_void_t(nkl),
                                .flags = 0,
                                .body_n = &NKS_FIRST(src->nodes),
                            },
                        .ir = {.alloc = nk_arena_getAllocator(&nkl->arena)},
                        .scope = &scope,
                    },
                .sym = sym,
                .type = proc_t,
                .kind = Entity_Proc,
            };
            ok = compileProc(mod, src, &proc_e);

            if (ok) {
                void (*proc)(void) = nkl_getSymbolAddress(mod, sym);
                ok = proc;

                if (ok) {
                    proc();
                }
            }
        }
    }
    return ok;
}
