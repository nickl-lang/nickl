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
    NklAstNode const *node;
    NkIrInstrDynArray ir;
    Scope *scope;
} ProcEntity;

typedef struct {
    union {
        ProcEntity proc;
    };
    NkAtom sym;
    NklType type;
    EntityKind kind;
} Entity;

typedef enum {
    Decl_None = 0,

    Decl_Entity,
    Decl_Extern,
    Decl_LocalVar,
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
    Entity *entity;
    NkAtom sym;
    NklType type;
    // Scope *scope;
    bool is_comptime;
} AstNodeExt;

typedef NkSlice(AstNodeExt) AstNodeExtArray;

typedef NkDynArray(Entity *) EntityPtrDynArray;

typedef struct {
    NkArena *scratch;

    NklState nkl;
    NklModule mod;

    NklType proc_t;
    NkIrInstrDynArray *instrs;
    u32 next_local_idx;

    NklSource src;
    AstNodeExtArray nodes_ext;

    Scope *scope_stack;

    EntityPtrDynArray procs_to_compile;
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

static AstNodeExt *nodeExtMut(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    return &ctx->nodes_ext.data[node_idx];
}

static AstNodeExt const *nodeExt(CompileCtx *ctx, NklAstNode const *node) {
    AstNodeExt const *node_ext = nodeExtMut(ctx, node);
    nk_assert(node_ext->type && "typecheck failed to compute type");
    return node_ext;
}

typedef struct {
    NklType type;
    NklTypeClass tclass;
} TypecheckArgs;

static bool typecheck(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args);

static bool typecheckComptimeConst(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    TRY(typecheck(ctx, node, args));

    AstNodeExt const *node_ext = nodeExt(ctx, node);
    if (!node_ext->is_comptime) {
        reportError(ctx, node, "comptime const expected");
        return false;
    }

    return true;
}

static NklAny compileComptimeConst(CompileCtx *ctx, NklAstNode const *node) {
    // AstNodeExt const *node_ext = nodeExt(ctx, node);

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

    else if (node->id == n_void) {
        *out_type = nkl_type_getVoid(ctx->nkl);
        return true;
    }

    reportError(ctx, node, "TODO: parseType is not finished");
    return false;
}

typedef struct {
    NkAtom sym;
    NklTypeArray param_types;
    NklType ret_t;
    u8 flags;
    NklAstNode const *body_n;
} ProcInfo;

static bool parseProcInfo(CompileCtx *ctx, NklAstNode const *node, ProcInfo *out_info) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    NklAstNode const *sym_n = nextNode(&it);
    NklAstNode const *params_n = nextNode(&it);
    NklAstNode const *ret_t_n = nextNode(&it);
    NklAstNode const *body_n = nextNode(&it);

    NkAtom const sym = parseId(ctx, sym_n);

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

    *out_info = (ProcInfo){
        .sym = sym,
        .param_types = {NKS_INIT(param_types)},
        .ret_t = ret_t,
        .flags = is_variadic ? NklProc_Variadic : 0,
        .body_n = body_n,
    };
    return true;
}

static bool typecheck(CompileCtx *ctx, NklAstNode const *node, TypecheckArgs const *args) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Typechecking node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt *node_ext = nodeExtMut(ctx, node);

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
                .sym = sym,
                .type = str_t,
                .is_comptime = true,
            };
            return true;
        }

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Decl const *found = DeclMap_find(&ctx->scope_stack->names, name);
            if (!found) {
                reportError(ctx, node, "undeclared identifier `%s`", nk_atom2cs(name));
                return false;
            }

            *node_ext = (AstNodeExt){
                .entity = found->kind == Decl_Entity ? found->entity : NULL,
                .type = found->type,
                .is_comptime = found->is_comptime,
            };
            return true;
        }

        case n_int: {
            NklType const type = (args->type && args->type->tclass == NklType_Numeric)
                                     ? args->type
                                     : nkl_type_getNumeric(ctx->nkl, Int64);
            *node_ext = (AstNodeExt){
                .type = type,
                .is_comptime = true,
            };
            return true;
        }

        case n_nullptr: {
            NklType const type =
                (args->type && args->type->tclass == NklType_Pointer)
                    ? args->type
                    : nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, nkl_type_getVoid(ctx->nkl), false);
            *node_ext = (AstNodeExt){
                .type = type,
                .is_comptime = true,
            };
            return true;
        }

        case n_list: {
            if (node->arity) {
                for (u32 i = 0; i < node->arity; i++) {
                    NklAstNode const *child_n = nextNode(&it);

                    TRY(typecheck(ctx, child_n, &(TypecheckArgs){0}));

                    AstNodeExt const *child_n_ext = nodeExt(ctx, child_n);
                    *node_ext = *child_n_ext;
                }
            } else {
                *node_ext = (AstNodeExt){
                    .type = nkl_type_getVoid(ctx->nkl),
                    .is_comptime = true,
                };
            }
            return true;
        }

        case n_call: {
            NklAstNode const *proc_n = nextNode(&it);
            NklAstNode const *args_n = nextNode(&it);

            TRY(typecheck(
                ctx,
                proc_n,
                &(TypecheckArgs){
                    .tclass = NklType_Procedure,
                }));

            AstNodeExt const *proc_node_ext = nodeExt(ctx, proc_n);

            if (proc_node_ext->type->tclass != NklType_Procedure) {
                reportError(ctx, proc_n, "proc expected");
                return false;
            }

            AstNodeIterator args_it = nodeIterate(ctx->src.nodes, args_n);

            bool const is_variadic = (proc_node_ext->type->as.proc.flags & NklProc_Variadic);
            bool const param_count = proc_node_ext->type->as.proc.param_types.size;

            if ((!is_variadic && args_n->arity != param_count) || (is_variadic && args_n->arity < param_count)) {
                reportError(ctx, proc_n, "invalid number of arguments");
                return false;
            }

            for (u32 i = 0; i < args_n->arity; i++) {
                NklAstNode const *arg_n = nextNode(&args_it);
                TRY(typecheck(
                    ctx,
                    arg_n,
                    &(TypecheckArgs){
                        .type = i < param_count ? proc_node_ext->type->as.proc.param_types.data[i] : NULL,
                    }));
            }

            *node_ext = (AstNodeExt){
                .type = proc_node_ext->type->as.proc.ret_t,
                .is_comptime = true,
            };
            return true;
        }

        case n_def: {
            NklAstNode const *pub_or_name_n = nextNode(&it);
            bool const is_pub = pub_or_name_n->id == n_pub;

            NklAstNode const *name_n = is_pub ? nextNode(&it) : pub_or_name_n;
            NklAstNode const *value_n = nextNode(&it);

            TRY(typecheckComptimeConst(ctx, value_n, &(TypecheckArgs){0}));

            AstNodeExt const *value_node_ext = nodeExt(ctx, value_n);

            nk_assert(value_node_ext->entity && "TODO: Is is_comptime == entity?");
            DeclMap_insert(
                &ctx->scope_stack->names,
                parseId(ctx, name_n),
                (Decl){
                    .entity = value_node_ext->entity,
                    .type = value_node_ext->type,
                    .kind = Decl_Entity,
                    .is_pub = is_pub,
                    .is_comptime = true,
                });

            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
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
                TRY(parseProcInfo(ctx, decl_n, &proc_info));

                NkIrTypeDynArray ir_param_types = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)};
                NK_ITERATE(NklType const *, it, proc_info.param_types) {
                    NklType type = *it;
                    nkda_append(&ir_param_types, nkl_type_getIrType(type));
                }

                NklType const proc_t = nkl_type_getProcedure(
                    ctx->nkl,
                    ctx->mod->com->word_size,
                    (NklProcInfo){
                        .param_types = {NKS_INIT_STRIDED(proc_info.param_types)},
                        .ret_t = proc_info.ret_t,
                        .flags = proc_info.flags,
                    });
                DeclMap_insert(
                    &ctx->scope_stack->names,
                    proc_info.sym,
                    (Decl){
                        .sym = proc_info.sym,
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
                                        .ret_type = nkl_type_getIrType(proc_info.ret_t),
                                        .flags = (proc_info.flags & NklProc_Variadic) ? NkIrProc_Variadic : 0,
                                    },
                                .lib = lib,
                                .kind = NkIrExtern_Proc,
                            },
                        .name = proc_info.sym,
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
            ProcInfo proc_info;
            TRY(parseProcInfo(ctx, node, &proc_info));

            NklType const proc_t = nkl_type_getProcedure(
                ctx->nkl,
                ctx->mod->com->word_size,
                (NklProcInfo){
                    .param_types = {NKS_INIT_STRIDED(proc_info.param_types)},
                    .ret_t = proc_info.ret_t,
                    .flags = proc_info.flags,
                });

            Entity *proc = nk_arena_allocT(&ctx->nkl->arena, Entity);
            *proc = (Entity){
                .proc =
                    {
                        .node = proc_info.body_n,
                        .ir = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)},
                        .scope = ctx->scope_stack,
                    },
                .sym = proc_info.sym,
                .type = proc_t,
                .kind = Entity_Proc,
            };

            *node_ext = (AstNodeExt){
                .entity = proc,
                .type = proc_t,
                .is_comptime = true,
            };
            return true;
        }

        case n_return: {
            if (node->arity) {
                NklAstNode const *arg_n = nextNode(&it);

                TRY(typecheck(
                    ctx,
                    arg_n,
                    &(TypecheckArgs){
                        .type = ctx->proc_t->as.proc.ret_t,
                    }));
            }

            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
        }

        case n_var: {
            NklAstNode const *name_n = nextNode(&it);
            NklAstNode const *type_n = nextNode(&it);
            NklAstNode const *val_n = nextNode(&it);

            NkAtom const name = parseId(ctx, name_n);

            NklType type = NULL;
            if (type_n->id) {
                TRY(parseType(ctx, type_n, &type));
            }

            TRY(typecheck(
                ctx,
                val_n,
                &(TypecheckArgs){
                    .type = type,
                }));

            if (!type) {
                AstNodeExt const *val_node_ext = nodeExt(ctx, val_n);
                type = val_node_ext->type;
            }

            DeclMap_insert(
                &ctx->scope_stack->names,
                name,
                (Decl){
                    .sym = name,
                    .type = type,
                    .kind = Decl_LocalVar,
                    .is_pub = false,
                    .is_comptime = false,
                });

            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
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

    Interm_Instr,
    Interm_Ref,
} IntermKind;

typedef struct {
    union {
        NkIrInstr instr;
        NkIrRef ref;
    };
    NklType type;
    IntermKind kind;
} Interm;

static NkAtom getNextLocal(CompileCtx *ctx) {
    return nk_s2atom(nk_tsprintf(ctx->scratch, "_%u", ctx->next_local_idx++));
}

static NkIrRef toRef(CompileCtx *ctx, Interm interm) {
    switch (interm.kind) {
        case Interm_Void:
            return (NkIrRef){0};

        case Interm_Ref:
            return interm.ref;

        case Interm_Instr: {
            NkIrInstr instr = interm.instr;
            if (instr.arg->kind == NkIrArg_None) {
                emit(ctx, instr);
                return (NkIrRef){0};
            } else {
                nk_assert(instr.arg[0].kind == NkIrArg_Ref);
                NkIrRef *dst = &instr.arg[0].ref;
                if (dst->kind == NkIrRef_None && interm.type->size) {
                    *dst = nkir_makeRefLocal(getNextLocal(ctx), &interm.type->ir_type);
                }
                emit(ctx, instr);
                return *dst;
            }
        }
    };

    nk_assert(!"unreachable");
    return (NkIrRef){0};
}

static Interm resolveDecl(CompileCtx *ctx, Decl const *decl) {
    switch (decl->kind) {
        case Decl_None:
            nk_assert(!"unreachable");
            return (Interm){0};

        case Decl_Entity: {
            nkda_append(&ctx->procs_to_compile, decl->entity);
            return (Interm){
                .ref = nkir_makeRefGlobal(decl->entity->sym, &decl->type->ir_type),
                .type = decl->type,
                .kind = Interm_Ref,
            };
        }

        case Decl_Extern: {
            return (Interm){
                .ref = nkir_makeRefGlobal(decl->sym, &decl->type->ir_type),
                .type = decl->type,
                .kind = Interm_Ref,
            };
        }

        case Decl_LocalVar: {
            NklType const void_ptr_t =
                nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, nkl_type_getVoid(ctx->nkl), false);
            return (Interm){
                .instr = nkir_make_load((NkIrRef){0}, nkir_makeRefLocal(decl->sym, &void_ptr_t->ir_type)),
                .type = decl->type,
                .kind = Interm_Instr,
            };
        }
    }
}

static void parseNumber(CompileCtx *ctx, void *addr, NklAstNode const *node, NkIrNumericValueType value_type) {
    NklToken const *token = getToken(ctx, node);
    NkString const token_str = nkl_getTokenStr(token, ctx->src.text);

    char const *cstr = nk_tprintf(ctx->scratch, NKS_FMT, NKS_ARG(token_str));

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
            if (nks_startsWith(token_str, nk_cs2s("0x"))) {
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
            if (nks_startsWith(token_str, nk_cs2s("0x"))) {
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

    nk_assert(endptr == cstr + token_str.size && "failed to parse numeric constant");
}

static Interm compile(CompileCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->src.nodes, node);
    NK_LOG_DBG("Compiling node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt const *node_ext = nodeExt(ctx, node);

    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    switch (node->id) {
        case n_extern:
        case n_def:
        case 0: {
            return (Interm){
                .type = nkl_type_getVoid(ctx->nkl),
                .kind = Interm_Void,
            };
        }

        case n_string:
        case n_escaped_string: {
            return (Interm){
                .ref = nkir_makeRefGlobal(node_ext->sym, &node_ext->type->ir_type),
                .type = node_ext->type,
                .kind = Interm_Ref,
            };
        }

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Decl const *found = DeclMap_find(&ctx->scope_stack->names, name);
            nk_assert(found);

            return resolveDecl(ctx, found);
        }

        case n_int: {
            NkIrImm imm = {0};
            nk_assert(node_ext->type->tclass == NklType_Numeric);
            parseNumber(ctx, &imm, node, node_ext->type->as.num.value_type);
            return (Interm){
                .ref = nkir_makeRefImm(imm, &node_ext->type->ir_type),
                .type = node_ext->type,
                .kind = Interm_Ref,
            };
        }

        case n_nullptr: {
            return (Interm){
                .ref = nkir_makeRefImm((NkIrImm){.u64 = 0}, &node_ext->type->ir_type),
                .type = node_ext->type,
                .kind = Interm_Ref,
            };
        }

        case n_list: {
            Interm res = {0};
            for (u32 i = 0; i < node->arity; i++) {
                toRef(ctx, res);
                res = compile(ctx, nextNode(&it));
            }
            return res;
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
                nkda_append(&args, toRef(ctx, compile(ctx, arg_n)));
            }

            return (Interm){
                .instr = nkir_make_call(
                    (NkIrRef){0}, // nkir_makeRefNull(&node_ext->type->ir_type), // TODO: Do we create null ref here?
                    toRef(ctx, proc),
                    (NkIrRefArray){NKS_INIT(args)}),
                .type = proc.type->as.proc.ret_t,
                .kind = Interm_Instr,
            };
        }

        case n_return: {
            NkIrRef arg_ref = {0};
            if (node->arity) {
                NklAstNode const *arg_n = nextNode(&it);

                Interm const arg = compile(ctx, arg_n);
                arg_ref = toRef(ctx, arg);
            }

            return (Interm){
                .instr = nkir_make_ret(arg_ref),
                .type = nkl_type_getVoid(ctx->nkl),
                .kind = Interm_Instr,
            };
        }

        case n_var: {
            NklAstNode const *name_n = nextNode(&it); // name
            nextNode(&it);                            // type
            NklAstNode const *val_n = nextNode(&it);

            NkAtom const name = parseId(ctx, name_n);
            Interm const val = compile(ctx, val_n);

            NklType const void_ptr_t =
                nkl_type_getPointer(ctx->nkl, ctx->mod->com->word_size, nkl_type_getVoid(ctx->nkl), false);

            Decl const *decl = DeclMap_find(&ctx->scope_stack->names, name);
            NkIrRef const var = nkir_makeRefLocal(name, &void_ptr_t->ir_type);

            emit(ctx, nkir_make_alloc(var, &decl->type->ir_type));
            emit(ctx, nkir_make_store(var, toRef(ctx, val)));

            return (Interm){
                .type = nkl_type_getVoid(ctx->nkl),
                .kind = Interm_Void,
            };
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

static bool compileProc(NklModule mod, NklSource const *src, Entity *proc);

static bool compileProcImpl(CompileCtx *ctx, Entity *proc) {
    TRY(typecheck(ctx, proc->proc.node, &(TypecheckArgs){0}));
    toRef(ctx, compile(ctx, proc->proc.node));

    if (!proc->proc.ir.size || NKS_LAST(proc->proc.ir).code != NkIrOp_ret) {
        if (proc->type->as.proc.ret_t->tclass != NklType_Void) {
            // TODO: Point to a better node
            reportError(ctx, proc->proc.node, "missing return statement");
            return false;
        }
        emit(ctx, nkir_make_ret((NkIrRef){0}));
    }

    while (ctx->procs_to_compile.size) {
        Entity *dep = NKS_LAST(ctx->procs_to_compile);
        nkda_pop(&ctx->procs_to_compile, 1);

        TRY(compileProc(ctx->mod, &ctx->src, dep));
    }

    TRY(nickl_defineSymbol(
        ctx->mod,
        &(NkIrSymbol){
            .proc =
                {
                    .params = {0},
                    .ret =
                        {
                            .type = &proc->type->as.proc.ret_t->ir_type,
                        },
                    .instrs = {NKS_INIT(proc->proc.ir)},
                    .flags = 0,
                },
            .name = proc->sym,
            .vis = NkIrVisibility_Hidden, // TODO: Prevent symbol being optimized away
            .kind = NkIrSymbol_Proc,
        }));

    return true;
}

static bool compileProc(NklModule mod, NklSource const *src, Entity *proc) {
    bool ok = true;

    nk_assert(proc->type->tclass == NklType_Procedure);

    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        CompileCtx ctx = {
            .scratch = scratch,

            .nkl = mod->com->nkl,
            .mod = mod,

            .proc_t = proc->type,
            .instrs = &proc->proc.ir,

            .src = *src,
            .nodes_ext =
                {
                    .data = nk_arena_allocTn(scratch, AstNodeExt, src->nodes.size),
                    .size = src->nodes.size,
                },

            .scope_stack = proc->proc.scope,

            .procs_to_compile = {.alloc = nk_arena_getAllocator(scratch)},
        };
        NKS_ZERO(ctx.nodes_ext);

        ok = compileProcImpl(&ctx, proc);
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
                    .ret_t = nkl_type_getVoid(nkl),
                    .flags = 0,
                });

            Scope scope = {
                .names = {.alloc = nk_arena_getAllocator(&nkl->arena)},
            };

            NkAtom const sym = nk_atom_unique((NkString){0});

            Entity proc = (Entity){
                .proc =
                    {
                        .node = &NKS_FIRST(src->nodes),
                        .ir = {.alloc = nk_arena_getAllocator(&nkl->arena)},
                        .scope = &scope,
                    },
                .sym = sym,
                .type = proc_t,
                .kind = Entity_Proc,
            };
            ok = compileProc(mod, src, &proc);

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
