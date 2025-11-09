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
    // Scope *scope;
    NkIrInstrDynArray ir;
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
} DeclKind;

typedef struct {
    union {
        Entity *entity;
    };
    DeclKind kind;
    bool is_pub;
    bool is_comptime;
} Decl;

NklType declType(Decl const *decl) {
    switch (decl->kind) {
        case Decl_None:
            nk_assert(!"unreachable");
            return NULL;

        case Decl_Entity:
            return decl->entity->type;
    }
}

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
    NklType type;
    // Scope *scope;
    bool is_comptime;
} AstNodeExt;

typedef NkSlice(AstNodeExt) AstNodeExtArray;

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

static bool typecheck(CompileCtx *ctx, NklAstNode const *node);

static bool typecheckComptimeConst(CompileCtx *ctx, NklAstNode const *node) {
    TRY(typecheck(ctx, node));

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

    reportError(ctx, node, "TODO: parseType is not finished");
    return false;
}

typedef struct {
    NkAtom sym;
    NklTypeArray param_types;
    NklType ret_t;
    u8 flags;
} ProcInfo;

static bool parseProcInfo(CompileCtx *ctx, NklAstNode const *node, ProcInfo *out_info) {
    AstNodeIterator it = nodeIterate(ctx->src.nodes, node);

    NklAstNode const *sym_n = nextNode(&it);
    NklAstNode const *params_n = nextNode(&it);
    NklAstNode const *ret_t_n = nextNode(&it);

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
    };
    return true;
}

static bool typecheck(CompileCtx *ctx, NklAstNode const *node) {
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

            Decl const *found = DeclMap_find(&ctx->scope_stack->names, name);
            if (!found) {
                reportError(ctx, node, "undeclared identifier `%s`", nk_atom2cs(name));
                return false;
            }

            *node_ext = (AstNodeExt){
                .entity = found->kind == Decl_Entity ? found->entity : NULL,
                .type = declType(found),
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

                AstNodeExt const *child_n_ext = nodeExt(ctx, child_n);
                *node_ext = *child_n_ext;
            }
            return true;
        }

        case n_call: {
            NklAstNode const *proc_n = nextNode(&it);
            NklAstNode const *args_n = nextNode(&it);

            TRY(typecheck(ctx, proc_n));

            AstNodeExt const *proc_node_ext = nodeExt(ctx, proc_n);

            if (proc_node_ext->type->tclass != NklType_Procedure) {
                reportError(ctx, proc_n, "proc expected");
                return false;
            }

            AstNodeIterator args_it = nodeIterate(ctx->src.nodes, args_n);

            // TODO: Typecheck args against proc_t
            for (u32 i = 0; i < args_n->arity; i++) {
                NklAstNode const *arg_n = nextNode(&args_it);
                TRY(typecheck(ctx, arg_n));
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

            TRY(typecheckComptimeConst(ctx, value_n));

            AstNodeExt const *value_node_ext = nodeExt(ctx, value_n);

            nk_assert(value_node_ext->entity && "TODO: Is is_comptime == entity?");
            DeclMap_insert(
                &ctx->scope_stack->names,
                parseId(ctx, name_n),
                (Decl){
                    .entity = value_node_ext->entity,
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

                // TODO: Declare extern procs
                NklType const proc_t = nkl_type_getProcedure(
                    ctx->nkl,
                    ctx->mod->com->word_size,
                    (NklProcInfo){
                        .param_types = {NKS_INIT_STRIDED(proc_info.param_types)},
                        .ret_t = proc_info.ret_t,
                        .flags = proc_info.flags,
                    });
                // DeclMap_insert(
                //     &ctx->scope_stack->names,
                //     proc_info.sym,
                //     (Entity){
                //         .type = proc_t,
                //         .kind = Entity_ExternProc,
                //         .is_pub = false,
                //         .is_comptime = false,
                //     });

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
                        .ir = {.alloc = nk_arena_getAllocator(&ctx->nkl->arena)},
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

        default: {
            reportError(ctx, node, "unknown AST node `%s`", nk_atom2cs(node->id));
            return false;
        }
    }

    nk_assert(!"unreachable");
    return false;
}

static void emit(CompileCtx *ctx, NkIrInstr const *instr) {
    nkda_append(ctx->instrs, *instr);
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

static NkIrRef toRef(CompileCtx *ctx, Interm interm) {
    switch (interm.kind) {
        case Interm_Void:
            return (NkIrRef){0};

        case Interm_Ref:
            return interm.ref;

        case Interm_Instr: {
            NkIrInstr instr = interm.instr;
            nk_assert(instr.arg[0].kind == NkIrArg_Ref);
            NkIrRef *dst = &instr.arg[0].ref;
            if (dst->kind == NkIrRef_None && interm.type->size) {
                NkAtom const sym = nk_s2atom(nk_tsprintf(ctx->scratch, "_%u", ctx->next_local_idx++));
                *dst = nkir_makeRefLocal(sym, &interm.type->ir_type);
            }
            emit(ctx, &instr);
            return *dst;
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
            NklType const type = declType(decl);
            return (Interm){
                .ref = nkir_makeRefGlobal(decl->entity->sym, &type->ir_type),
                .type = type,
                .kind = Interm_Ref,
            };
        }
    }
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

        case n_id: {
            NkAtom const name = parseId(ctx, node);

            Decl const *found = DeclMap_find(&ctx->scope_stack->names, name);
            nk_assert(found);

            return resolveDecl(ctx, found);
        }

        case n_int: {
            NklToken const *token = getToken(ctx, node);
            NkString const token_str = nkl_getTokenStr(token, ctx->src.text);
            char const *cstr = nk_tprintf(ctx->scratch, NKS_FMT, NKS_ARG(token_str));
            char *endptr = NULL;
            i64 const val = strtoll(cstr, &endptr, 0);
            return (Interm){
                .ref = nkir_makeRefImm((NkIrImm){.i64 = val}, &node_ext->type->ir_type),
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

static bool compileProc(
    NklModule mod,
    NklSource const *src,
    NklAstNode const *node,
    NklType proc_t,
    NkIrInstrDynArray *instrs) {
    bool ok = true;

    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        CompileCtx ctx = {
            .scratch = scratch,

            .nkl = mod->com->nkl,
            .mod = mod,

            .proc_t = proc_t,
            .instrs = instrs,

            .src = *src,
            .nodes_ext =
                {
                    .data = nk_arena_allocTn(scratch, AstNodeExt, src->nodes.size),
                    .size = src->nodes.size,
                },
        };
        NKS_ZERO(ctx.nodes_ext);

        pushScope(&ctx);

        ok = typecheck(&ctx, node);
        if (ok) {
            toRef(&ctx, compile(&ctx, node));
        }
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

            NkIrInstrDynArray instrs = {.alloc = nk_arena_getAllocator(&nkl->arena)};

            ok = compileProc(mod, src, &NKS_FIRST(src->nodes), proc_t, &instrs);

            if (ok) {
                NkAtom const sym = nk_atom_unique((NkString){0});
                ok = nickl_defineSymbol(
                    mod,
                    &(NkIrSymbol){
                        .proc =
                            {
                                .params = {0},
                                .ret =
                                    {
                                        .type = &nkl_type_getVoid(nkl)->ir_type,
                                    },
                                .instrs = {NKS_INIT(instrs)},
                                .flags = 0,
                            },
                        .name = sym,
                        .vis = NkIrVisibility_Local,
                        .kind = NkIrSymbol_Proc,
                    });

                if (ok) {
                    NKSB_FIXED_BUFFER(name, 128);
                    nkir_printSymbolName(nksb_getStream(&name), sym);
                    void (*proc)(void) = nkl_getSymbolAddress(mod, (NkString){NKS_INIT(name)});
                    ok = proc;

                    if (ok) {
                        proc();
                    }
                }
            }
        }
    }
    return ok;
}
