#include "nkl/core/compiler.h"

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/os/path.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

struct Void {};

#define CHECK(EXPR)            \
    EXPR;                      \
    if (nkl_getErrorCount()) { \
        return {};             \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(nkda_append((AR), (VAL)))

} // namespace

enum DeclKind {
    DeclKind_Undefined,

    DeclKind_Comptime,
    DeclKind_ComptimeIncomplete,
    DeclKind_ComptimeUnresolved,
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
            nkltype_t type;
        } local;
        struct {
            Scope *scope;
            nklval_t val;
        } module;
        struct {
            nkltype_t type;
        } param;
    } as;
    DeclKind kind;
};

static u64 nk_atom_hash(NkAtom const key) {
    return nk_hashVal(key);
}
static bool nk_atom_equal(NkAtom const lhs, NkAtom const rhs) {
    return lhs == rhs;
}

struct Decl_kv {
    NkAtom key;
    Decl val;
};
static NkAtom const *Decl_kv_GetKey(Decl_kv const *item) {
    return &item->key;
}
NK_HASH_TREE_DEFINE(DeclMap, Decl_kv, NkAtom, Decl_kv_GetKey, nk_atom_hash, nk_atom_equal);

struct Scope {
    Scope *next;

    NkArena *main_arena;
    NkArena *temp_arena;
    NkArenaFrame temp_frame;

    DeclMap locals;
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
    nkltype_t proc_t;
    Context *ctx;
};

struct FileContext_kv {
    NkAtom key;
    FileContext val;
};
static NkAtom const *FileContext_kv_GetKey(FileContext_kv const *item) {
    return &item->key;
}
NK_HASH_TREE_DEFINE(FileMap, FileContext_kv, NkAtom, FileContext_kv_GetKey, nk_atom_hash, nk_atom_equal);

struct NklCompiler_T {
    NklState nkl;
    NkArena perm_arena;
    NkArena temp_arenas[2];
    FileMap files;
    NklErrorState errors;
};

static NkArena *getNextTempArena(NklCompiler c, NkArena *conflict) {
    return &c->temp_arenas[0] == conflict ? &c->temp_arenas[1] : &c->temp_arenas[0];
}

FileContext &getContextForFile(NklCompiler c, NkAtom file) {
    auto found = FileMap_find(&c->files, file);
    if (!found) {
        found = FileMap_insert(&c->files, {file, {}});
    }
    return found->val;
}

struct Ref {
    bool tmp_nonempty;
    nkltype_t type;
};

enum ValueKind {
    ValueKind_Void,

    ValueKind_Const,
    ValueKind_Decl,
    ValueKind_Instr,
    ValueKind_Module,
    ValueKind_Ref,
};

struct ValueInfo {
    union {
        struct {
            void *data;
        } cnst;
        Decl *decl;
        struct {
            Ref dst;
            char const *tmp_instr_name;
        } instr;
        struct {
            void *data;
            Scope *scope;
        } module;
        Ref ref;
    } as;
    nkltype_t type;
    ValueKind kind;
};

struct NklModule_T {
    NklCompiler c;
};

static Ref asRef(ValueInfo const &val) {
    Ref ref{};

    switch (val.kind) {
        case ValueKind_Void:
            break;

        case ValueKind_Const:
            // TODO: Actually create a const ref
            ref = {true, val.type};
            break;

        case ValueKind_Decl:
            // TODO: Actually create a ref from decl
            ref = {true, val.type};
            break;

        case ValueKind_Instr: {
            auto instr = val.as.instr;
            auto &dst = instr.dst;
            if (!dst.tmp_nonempty && nklt_sizeof(val.type)) {
                // TODO: Actually replace dst with a local ref
                dst = {true, val.type};
            }
            // TODO: Actually emit instructions
            printf("%s\n", instr.tmp_instr_name);
            ref = dst;
            break;
        }

        case ValueKind_Module:
            ref = {true, val.type};
            break;

        case ValueKind_Ref:
            ref = val.as.ref;
            break;

        default:
            nk_assert(!"unreachable");
            return {};
    }

    return ref;
}

static ValueInfo compileNode(Context &ctx, usize node_idx);
static Void compileStmt(Context &ctx, usize node_idx);
static bool compileAst(Context &ctx);

usize parentNodeIdx(Context &ctx) {
    return ctx.node_stack->next ? ctx.node_stack->next->node_idx : -1u;
}

static void pushScope(Context &ctx, NkArena *main_arena, NkArena *temp_arena) {
    auto scope = new (nk_arena_allocT<Scope>(main_arena)) Scope{
        .next{},

        .main_arena = main_arena,
        .temp_arena = temp_arena,
        .temp_frame = nk_arena_grab(temp_arena),

        .locals{nullptr, nk_arena_getAllocator(main_arena)},
    };
    nk_list_push(ctx.scope_stack, scope);
}

static void popScope(Context &ctx) {
    nk_assert(ctx.scope_stack && "no current scope");
    nk_arena_popFrame(ctx.scope_stack->temp_arena, ctx.scope_stack->temp_frame);
    nk_list_pop(ctx.scope_stack);
}

static Decl &makeDecl(Context &ctx, NkAtom name) {
    nk_assert(ctx.scope_stack && "no current scope");
    NK_LOG_DBG("Declaring name=`%s` scope=%p", nk_atom2cs(name), (void *)ctx.scope_stack);
    // TODO: Check for name conflict
    auto kv = DeclMap_insert(&ctx.scope_stack->locals, {name, {}});
    return kv->val;
}

static void defineComptime(Context &ctx, NkAtom name, nklval_t val) {
    makeDecl(ctx, name) = {{.comptime{.val = val}}, DeclKind_Comptime};
}

static void defineComptimeUnresolved(Context &ctx, NkAtom name, usize node_idx) {
    auto ctx_copy = nk_arena_allocT<Context>(ctx.scope_stack->main_arena);
    *ctx_copy = ctx;
    makeDecl(ctx, name) = {{.comptime_unresolved{.ctx = ctx_copy, .node_idx = node_idx}}, DeclKind_ComptimeUnresolved};
}

static void defineLocal(Context &ctx, NkAtom name, nkltype_t type) {
    makeDecl(ctx, name) = {{.local{.type = type}}, DeclKind_Local};
}

static void defineParam(Context &ctx, NkAtom name, nkltype_t type) {
    makeDecl(ctx, name) = {{.param{.type = type}}, DeclKind_Param};
}

static Decl &resolve(Scope *scope, NkAtom name) {
    NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)scope);

    for (; scope; scope = scope->next) {
        auto found = DeclMap_find(&scope->locals, name);
        if (found) {
            return found->val;
        }
    }

    static Decl s_undefined_decl{{}, DeclKind_Undefined};
    return s_undefined_decl;
}

NklCompiler nkl_createCompiler(NklState nkl, NklTargetTriple target) {
    NkArena arena{};
    NklCompiler_T *c = new (nk_arena_allocT<NklCompiler_T>(&arena)) NklCompiler_T{
        .nkl = nkl,
        .perm_arena = arena,
        .temp_arenas = {},
        .files = {},
        .errors = {},
    };
    c->files.alloc = nk_arena_getAllocator(&c->perm_arena);
    c->errors.arena = &c->perm_arena;
    return c;
}

void nkl_freeCompiler(NklCompiler c) {
    nk_arena_free(&c->temp_arenas[0]);
    nk_arena_free(&c->temp_arenas[1]);

    auto perm_arena = c->perm_arena;
    nk_arena_free(&perm_arena);
}

usize nkl_getCompileErrorCount(NklCompiler c) {
    return c->errors.error_count;
}

NklError *nkl_getCompileErrorList(NklCompiler c) {
    return c->errors.errors;
}

NklModule nkl_createModule(NklCompiler c) {
    return new (nk_allocT<NklModule_T>(nk_arena_getAllocator(&c->perm_arena))) NklModule_T{
        .c = c,
    };
}

void nkl_writeModule(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);
}

static bool isValueKnown(ValueInfo const &val) {
    return val.kind == ValueKind_Void || val.kind == ValueKind_Const ||
           (val.kind == ValueKind_Decl &&
            (val.as.decl->kind == DeclKind_Comptime || val.as.decl->kind == DeclKind_Module)) ||
           val.kind == ValueKind_Module;
}

static nklval_t getValueFromInfo(ValueInfo const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");

    switch (val.kind) {
        case ValueKind_Void:
            return {nullptr, val.type};
        case ValueKind_Const:
            return {val.as.cnst.data, val.type};
        case ValueKind_Decl: {
            switch (val.as.decl->kind) {
                case DeclKind_Comptime:
                    return val.as.decl->as.comptime.val;
                case DeclKind_Module:
                    return val.as.decl->as.module.val;
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
        }
        case ValueKind_Module:
            return {val.as.cnst.data, val.type};
        default:
            nk_assert(!"unreachable");
            return {};
    }
}
NK_PRINTF_LIKE(2) static ValueInfo error(Context &ctx, char const *fmt, ...) {
    auto src = ctx.src;
    auto last_node = ctx.node_stack;
    auto err_token = last_node ? &src->tokens.data[src->nodes.data[last_node->node_idx].token_idx] : nullptr;

    va_list ap;
    va_start(ap, fmt);
    nkl_vreportError(ctx.src->file, err_token, fmt, ap);
    va_end(ap);

    return {};
}

static ValueInfo resolveComptime(Decl &decl) {
    nk_assert(decl.kind == DeclKind_ComptimeUnresolved);

    auto &ctx = *decl.as.comptime_unresolved.ctx;
    auto node_idx = decl.as.comptime_unresolved.node_idx;

    decl.as = {};
    decl.kind = DeclKind_ComptimeIncomplete;

    NK_LOG_DBG("Resolving comptime const: node %zu file=`%s`", node_idx, nk_atom2cs(ctx.src->file));

    DEFINE(val, compileNode(ctx, node_idx));

    if (!isValueKnown(val)) {
        // TODO: Improve error message
        return error(ctx, "value is not known");
    }

    if (decl.kind == DeclKind_Module || decl.kind == DeclKind_ModuleIncomplete) {
        decl.as.module.val = getValueFromInfo(val);

        return {{.decl = &decl}, decl.as.module.val.type, ValueKind_Decl};
    } else {
        decl.as.comptime.val = getValueFromInfo(val);
        decl.kind = DeclKind_Comptime;

        return {{.decl = &decl}, decl.as.comptime.val.type, ValueKind_Decl};
    }
}

static ValueInfo resolveDecl(Decl &decl) {
    switch (decl.kind) {
        case DeclKind_Comptime:
            return {{.decl = &decl}, decl.as.comptime.val.type, ValueKind_Decl};
        case DeclKind_ComptimeUnresolved:
            return resolveComptime(decl);
        case DeclKind_Local:
            return {{.decl = &decl}, decl.as.local.type, ValueKind_Decl};
        case DeclKind_Module:
        case DeclKind_ModuleIncomplete:
            return {{.decl = &decl}, decl.as.module.val.type, ValueKind_Decl};
        case DeclKind_Param:
            return {{.decl = &decl}, decl.as.local.type, ValueKind_Decl};
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

static NkAtom getFileId(NkString filename) {
    NKSB_FIXED_BUFFER(filename_nt, NK_MAX_PATH);
    nksb_printf(&filename_nt, NKS_FMT, NKS_ARG(filename));
    nksb_appendNull(&filename_nt);

    NkString canonical_file_path_s;
    char canonical_file_path[NK_MAX_PATH] = {};
    if (nk_fullPath(canonical_file_path, filename_nt.data) >= 0) {
        canonical_file_path_s = nk_cs2s(canonical_file_path);
    } else {
        canonical_file_path_s = filename;
    }

    return nk_s2atom(canonical_file_path_s);
}

static bool isModule(ValueInfo const &val) {
    return val.kind == ValueKind_Module ||
           (val.kind == ValueKind_Decl &&
            (val.as.decl->kind == DeclKind_Module || val.as.decl->kind == DeclKind_ModuleIncomplete));
}

static Scope *getModuleScope(ValueInfo const &val) {
    nk_assert(isModule(val) && "module expected");
    return val.kind == ValueKind_Module ? val.as.module.scope : val.as.decl->as.module.scope;
}

static FileContext *importFile(NklCompiler c, NkString filename, NkArena *main_arena, NkArena *temp_arena, Decl *decl) {
    auto nkl = c->nkl;

    auto file = getFileId(filename);

    DEFINE(src, nkl_getSource(nkl, file));

    // TODO: Hardcoded word size
    auto proc_t = nkl_get_proc(
        nkl,
        8,
        NklProcInfo{
            .param_types = {},
            .ret_t = nkl_get_void(nkl),
            .call_conv = NkCallConv_Nk,
            .flags = {},
        });

    auto &file_ctx = getContextForFile(c, file);
    if (!file_ctx.ctx) {
        file_ctx = {
            .proc_t = proc_t,
            .ctx = nk_arena_allocT<Context>(&c->perm_arena),
        };
        auto &ctx = *file_ctx.ctx;
        ctx = {
            .m = nkl_createModule(c),
            .src = src,
            .scope_stack = {},
            .node_stack = {},
        };

        pushScope(ctx, main_arena, temp_arena);

        if (decl) {
            decl->as.module.val.type = proc_t;
            decl->as.module.scope = file_ctx.ctx->scope_stack;
            decl->kind = DeclKind_ModuleIncomplete;
        }

        CHECK(compileAst(ctx));
    } else if (decl) {
        decl->as.module.val.type = proc_t;
        decl->as.module.scope = file_ctx.ctx->scope_stack;
        decl->kind = DeclKind_ModuleIncomplete;
    }

    return &file_ctx;
}

static ValueInfo store(Ref const &dst, ValueInfo src) {
    auto const dst_type = dst.type;
    auto const src_type = src.type;
    if (nklt_sizeof(src_type)) {
        if (src.kind == ValueKind_Instr && !src.as.instr.dst.tmp_nonempty) {
            src.as.instr.dst = dst;
        } else {
            asRef(src);
            // TODO: Returning fake mov instr
            src = {{.instr{{true, dst_type}, "mov"}}, dst_type, ValueKind_Instr};
        }
    }
    auto const src_ref = asRef(src);
    return {{.ref = src_ref}, src_ref.type, ValueKind_Ref};
}

static ValueInfo getLvalueRef(Context &ctx, usize node_idx) {
    auto const &node = ctx.src->nodes.data[node_idx];

    if (node.id == n_id) {
        auto token = &ctx.src->tokens.data[node.token_idx];
        auto name_str = nkl_getTokenStr(token, ctx.src->text);
        auto name = nk_s2atom(name_str);
        auto &decl = resolve(ctx.scope_stack, name);
        switch (decl.kind) {
            case DeclKind_Undefined:
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));

            case DeclKind_Local:
                // TODO: Returning fake ref, make frame ref
                return {{.ref = {true, decl.as.local.type}}, decl.as.local.type, ValueKind_Ref};

            case DeclKind_Comptime:
            case DeclKind_ComptimeIncomplete:
            case DeclKind_ComptimeUnresolved:
            case DeclKind_Module:
            case DeclKind_ModuleIncomplete:
            case DeclKind_Param:
                return error(ctx, "cannot assign to `" NKS_FMT "`", NKS_ARG(name_str));
            default:
                nk_assert(!"unreachable");
                return {};
        }
    } else {
        // TODO: Improve error msg
        return error(ctx, "invalid lvalue");
    }
}

static ValueInfo compileNode(Context &ctx, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto &src = *ctx.src;
    auto m = ctx.m;
    auto c = m->c;
    auto nkl = c->nkl;

    auto temp_frame = nk_arena_grab(ctx.scope_stack->temp_arena);
    defer {
        nk_arena_popFrame(ctx.scope_stack->temp_arena, temp_frame);
    };

    auto new_list_node = nk_arena_allocT<NodeListNode>(ctx.scope_stack->main_arena);
    new_list_node->node_idx = node_idx;
    nk_list_push(ctx.node_stack, new_list_node);
    defer {
        nk_list_pop(ctx.node_stack);
    };

    auto const &node = src.nodes.data[node_idx];

    NK_LOG_DBG("Compiling node %zu #%s", node_idx, nk_atom2cs(node.id));

    auto next_idx = node_idx + 1;

    auto get_next_child = [&src](usize &next_child_idx) {
        auto child_idx = next_child_idx;
        next_child_idx = nkl_ast_nextChild(src.nodes, next_child_idx);
        return child_idx;
    };

    Decl *decl = nullptr;
    NkAtom decl_name;
    auto parent_idx = parentNodeIdx(ctx);
    if (parent_idx != -1u && src.nodes.data[parent_idx].id == n_const) {
        auto next_idx = parent_idx + 1;

        auto name_idx = get_next_child(next_idx);
        auto const &name_n = src.nodes.data[name_idx];
        decl_name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));
        decl = &resolve(ctx.scope_stack, decl_name);

        nk_assert(decl->kind == DeclKind_ComptimeIncomplete);
    }

    switch (node.id) {
        case n_add: {
            auto lhs_idx = get_next_child(next_idx);
            auto rhs_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));
            asRef(lhs);
            DEFINE(rhs, compileNode(ctx, rhs_idx));
            asRef(rhs);
            // TODO: Assuming equal and correct types in add
            return {{.instr{{}, "add"}}, lhs.type, ValueKind_Instr};
        }

        case n_assign: {
            auto lhs_idx = get_next_child(next_idx);
            auto rhs_idx = get_next_child(next_idx);

            DEFINE(lhs, getLvalueRef(ctx, lhs_idx));
            DEFINE(rhs, compileNode(ctx, rhs_idx));

            return store(asRef(lhs), rhs);
        }

        case n_call: {
            auto lhs_idx = get_next_child(next_idx);
            auto args_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));

            if (lhs.type->tclass != NklType_Procedure) {
                // TODO: Improve error message
                return error(ctx, "procedure expected");
            }

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);
            NkDynArray(ValueInfo) args{NKDA_INIT(temp_alloc)};

            auto const &args_n = src.nodes.data[args_idx];
            auto next_arg_idx = args_idx + 1;
            for (size_t i = 0; i < args_n.arity; i++) {
                auto arg_idx = get_next_child(next_arg_idx);
                APPEND(&args, compileNode(ctx, arg_idx));
            }

            return {{.instr{{}, "call"}}, (nkltype_t)lhs.type->ir_type.as.proc.info.ret_t, ValueKind_Instr};
        }

        case n_const: {
            auto name_idx = get_next_child(next_idx);
            auto value_idx = get_next_child(next_idx);

            auto const &name_n = src.nodes.data[name_idx];
            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            CHECK(defineComptimeUnresolved(ctx, name, value_idx));

            return ValueInfo{};
        }

        case n_context: {
            auto lhs_idx = get_next_child(next_idx);
            auto name_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));

            if (!isModule(lhs)) {
                // TODO: Improve error message
                return error(ctx, "module expected");
            }

            auto scope = getModuleScope(lhs);

            auto const &name_n = src.nodes.data[name_idx];
            auto name_token = &src.tokens.data[name_n.token_idx];
            auto name_str = nkl_getTokenStr(name_token, src.text);
            auto name = nk_s2atom(name_str);

            NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)scope);

            auto found = DeclMap_find(&scope->locals, name);
            if (found) {
                return resolveDecl(found->val);
            } else {
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));
            }

            break;
        }

        case n_escaped_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
            nks_unescape(nksb_getStream(&sb), text);
            NkString const unescaped_text{NKS_INIT(sb)};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, unescaped_text.size + 1);
            // TODO: Hardcoded word size
            auto str_t = nkl_get_ptr(nkl, 8, ar_t, true);

            auto str = nks_copyNt(nk_arena_getAllocator(&c->perm_arena), unescaped_text);
            return ValueInfo{{.cnst{(void *)str.data}}, str_t, ValueKind_Const};
        }

        case n_extern_c_proc: {
            auto params_idx = get_next_child(next_idx);
            auto ret_t_idx = get_next_child(next_idx);

            // TODO: Boilerplate with n_proc
            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

            NkDynArray(nkltype_t) param_types{NKDA_INIT(temp_alloc)};

            u8 proc_flags = 0;

            auto const &params_n = src.nodes.data[params_idx];
            auto next_param_idx = params_idx + 1;
            for (size_t i = 0; i < params_n.arity; i++) {
                auto param_idx = get_next_child(next_param_idx);

                auto const &param_n = src.nodes.data[param_idx];
                if (param_n.id == n_ellipsis) {
                    proc_flags |= NkProcVariadic;
                    // TODO: Check that ellipsis is the last param
                    break;
                }

                auto next_idx = param_idx + 1;

                auto name_idx = get_next_child(next_idx);
                auto type_idx = get_next_child(next_idx);

                DEFINE(type_v, compileNode(ctx, type_idx));

                if (type_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(type_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                auto type = nklval_as(nkltype_t, getValueFromInfo(type_v));

                nkda_append(&param_types, type);
            }

            DEFINE(ret_t_v, compileNode(ctx, ret_t_idx));

            // TODO: Boilerplate in type checking
            if (ret_t_v.type->tclass != NklType_Typeref) {
                // TODO: Improve error message
                return error(ctx, "type expected");
            }

            if (!isValueKnown(ret_t_v)) {
                // TODO: Improve error message
                return error(ctx, "value is not known");
            }

            auto ret_t = nklval_as(nkltype_t, getValueFromInfo(ret_t_v));

            // TODO: Hardcoded word size
            auto proc_t = nkl_get_proc(
                nkl,
                8,
                NklProcInfo{
                    .param_types = {NK_SLICE_INIT(param_types)},
                    .ret_t = ret_t,
                    .call_conv = NkCallConv_Cdecl,
                    .flags = proc_flags,
                });

            // TODO: Returning null as proc value
            return {{.module{.data = nullptr, .scope = ctx.scope_stack}}, proc_t, ValueKind_Module};
        }

        case n_i8: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Int8);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_i16: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Int16);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_i32: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Int32);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_i64: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Int32);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_f32: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Float32);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_f64: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Float32);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_id: {
            auto token = &src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            auto name = nk_s2atom(name_str);

            auto &decl = resolve(ctx.scope_stack, name);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));
            } else {
                return resolveDecl(decl);
            }
        }

        case n_import: {
            auto path_idx = get_next_child(next_idx);
            auto &path_n = src.nodes.data[path_idx];
            auto path_str = nkl_getTokenStr(&src.tokens.data[path_n.token_idx], src.text);
            NkString const path{path_str.data + 1, path_str.size - 2};

            NKSB_FIXED_BUFFER(path_buf, NK_MAX_PATH);
            nksb_printf(&path_buf, NKS_FMT, NKS_ARG(path));
            nksb_appendNull(&path_buf);

            NkString import_path{};

            if (nk_pathIsRelative(path_buf.data)) {
                auto parent = nk_path_getParent(nk_atom2s(src.file));

                nksb_clear(&path_buf);
                nksb_printf(&path_buf, NKS_FMT "%c" NKS_FMT, NKS_ARG(parent), NK_PATH_SEPARATOR, NKS_ARG(path));

                import_path = {NKS_INIT(path_buf)};
            } else {
                import_path = path;
            }

            DEFINE(
                file_ctx, importFile(c, import_path, ctx.scope_stack->main_arena, ctx.scope_stack->temp_arena, decl));

            // TODO: Returning null as proc value
            return {
                {.module{.data = nullptr, .scope = file_ctx->ctx->scope_stack}}, file_ctx->proc_t, ValueKind_Module};
        }

        case n_int: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            auto pvalue = nk_arena_allocT<int64_t>(&c->perm_arena);
            // TODO: Replace sscanf in compiler
            int res = sscanf(token_str.data, "%" SCNi64, pvalue);
            nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
            return ValueInfo{{.cnst{pvalue}}, nkl_get_numeric(nkl, Int64), ValueKind_Const};
        }

        case n_less: {
            auto lhs_idx = get_next_child(next_idx);
            auto rhs_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));
            asRef(lhs);
            DEFINE(rhs, compileNode(ctx, rhs_idx));
            asRef(rhs);
            // TODO: Assuming equal and correct types in add
            return {{.instr{{}, "cmp lt"}}, nkl_get_bool(nkl), ValueKind_Instr};
        }

        case n_list: {
            for (size_t i = 0; i < node.arity; i++) {
                CHECK(compileStmt(ctx, get_next_child(next_idx)));
            }
            break;
        }

        case n_member: {
            auto lhs_idx = get_next_child(next_idx);
            auto rhs_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));

            if (lhs.type->tclass != NklType_Struct) {
                // TODO: Improve error message
                return error(ctx, "struct expected");
            }

            auto &name_n = src.nodes.data[rhs_idx];
            auto token = &src.tokens.data[name_n.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            auto name = nk_s2atom(name_str);

            NklField const *found_field = nullptr;

            for (auto &field : nk_iterate(lhs.type->as.strct.fields)) {
                if (field.name == name) {
                    found_field = &field;
                }
            }

            if (!found_field) {
                // TODO: Improve error msg
                NKSB_FIXED_BUFFER(sb, 1024);
                nkl_type_inspect(lhs.type, nksb_getStream(&sb));
                return error(ctx, "no field named `" NKS_FMT "` in `" NKS_FMT "`", NKS_ARG(name_str), NKS_ARG(sb));
            }

            // TODO: Returning fake ref
            return {{.ref = {true, found_field->type}}, found_field->type, ValueKind_Ref};
        }

        case n_mul: {
            auto lhs_idx = get_next_child(next_idx);
            auto rhs_idx = get_next_child(next_idx);

            DEFINE(lhs, compileNode(ctx, lhs_idx));
            asRef(lhs);
            DEFINE(rhs, compileNode(ctx, rhs_idx));
            asRef(rhs);
            // TODO: Assuming equal and correct types in mul
            return {{.instr{{}, "mul"}}, lhs.type, ValueKind_Instr};
        }

        case n_proc: {
            auto main_arena = decl ? ctx.scope_stack->main_arena : ctx.scope_stack->temp_arena;
            auto temp_arena = decl ? ctx.scope_stack->temp_arena : getNextTempArena(c, ctx.scope_stack->temp_arena);

            auto params_idx = get_next_child(next_idx);
            auto ret_t_idx = get_next_child(next_idx);
            auto body_idx = get_next_child(next_idx);

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

            NkDynArray(NkAtom) param_names{NKDA_INIT(temp_alloc)};
            NkDynArray(nkltype_t) param_types{NKDA_INIT(temp_alloc)};

            auto const &params_n = src.nodes.data[params_idx];
            auto next_param_idx = params_idx + 1;
            for (size_t i = 0; i < params_n.arity; i++) {
                auto param_idx = get_next_child(next_param_idx) + 1;

                auto name_idx = get_next_child(param_idx);
                auto type_idx = get_next_child(param_idx);

                auto name_n = &src.nodes.data[name_idx];
                auto name_token = &src.tokens.data[name_n->token_idx];
                auto name_str = nkl_getTokenStr(name_token, src.text);
                auto name = nk_s2atom(name_str);

                DEFINE(type_v, compileNode(ctx, type_idx));

                if (type_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(type_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                auto type = nklval_as(nkltype_t, getValueFromInfo(type_v));

                nkda_append(&param_names, name);
                nkda_append(&param_types, type);
            }

            DEFINE(ret_t_v, compileNode(ctx, ret_t_idx));

            // TODO: Boilerplate in type checking
            if (ret_t_v.type->tclass != NklType_Typeref) {
                // TODO: Improve error message
                return error(ctx, "type expected");
            }

            if (!isValueKnown(ret_t_v)) {
                // TODO: Improve error message
                return error(ctx, "value is not known");
            }

            auto ret_t = nklval_as(nkltype_t, getValueFromInfo(ret_t_v));

            // TODO: Hardcoded word size
            auto proc_t = nkl_get_proc(
                nkl,
                8,
                NklProcInfo{
                    .param_types = {NK_SLICE_INIT(param_types)},
                    .ret_t = ret_t,
                    .call_conv = NkCallConv_Nk,
                    .flags = {},
                });

            pushScope(ctx, main_arena, temp_arena);
            defer {
                popScope(ctx);
            };

            if (decl) {
                decl->as.module.val.type = proc_t;
                decl->as.module.scope = ctx.scope_stack;
                decl->kind = DeclKind_ModuleIncomplete;
            }

            for (usize i = 0; i < params_n.arity; i++) {
                CHECK(defineParam(ctx, param_names.data[i], param_types.data[i]));
            }

            printf("%s {\n", decl ? nk_atom2cs(decl_name) : "anonymous_proc");
            CHECK(compileStmt(ctx, body_idx));
            printf("}\n");

            // TODO: Returning null as proc value
            return {{.module{.data = nullptr, .scope = ctx.scope_stack}}, proc_t, ValueKind_Module};
        }

        case n_ptr: {
            auto target_idx = get_next_child(next_idx);

            DEFINE(target, compileNode(ctx, target_idx));

            // TODO: Boilerplate in type checking
            if (target.type->tclass != NklType_Typeref) {
                // TODO: Improve error message
                return error(ctx, "type expected");
            }

            if (!isValueKnown(target)) {
                // TODO: Improve error message
                return error(ctx, "value is not known");
            }

            auto target_t = nklval_as(nkltype_t, getValueFromInfo(target));

            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            // TODO: Hardcoded word size
            *pvalue = nkl_get_ptr(nkl, 8, target_t, false);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_return: {
            // TODO: Typecheck the returned value
            if (node.arity) {
                auto arg_idx = get_next_child(next_idx);
                DEFINE(arg, compileNode(ctx, arg_idx));
                asRef(arg);
            } else {
            }
            printf("return\n");
            return {};
        }

        case n_run: {
            // TODO: Actually run code
            printf("RUN\n");
            return {};
        }

        case n_scope: {
            pushScope(ctx, ctx.scope_stack->temp_arena, getNextTempArena(c, ctx.scope_stack->temp_arena));
            defer {
                popScope(ctx);
            };

            auto child_idx = get_next_child(next_idx);
            CHECK(compileStmt(ctx, child_idx));

            return ValueInfo{};
        }

        case n_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, text.size + 1);
            // TODO: Hardcoded word size
            auto str_t = nkl_get_ptr(nkl, 8, ar_t, true);

            auto str = nks_copyNt(nk_arena_getAllocator(&c->perm_arena), text);
            return ValueInfo{{.cnst{(void *)str.data}}, str_t, ValueKind_Const};
        }

        case n_struct: {
            auto params_idx = get_next_child(next_idx);

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

            NkDynArray(NklField) fields{NKDA_INIT(temp_alloc)};

            // TODO: Boilerplate in param compilation
            auto const &params_n = src.nodes.data[params_idx];
            auto next_param_idx = params_idx + 1;
            for (size_t i = 0; i < params_n.arity; i++) {
                auto param_idx = get_next_child(next_param_idx) + 1;

                auto name_idx = get_next_child(param_idx);
                auto type_idx = get_next_child(param_idx);

                auto name_n = &src.nodes.data[name_idx];
                auto name_token = &src.tokens.data[name_n->token_idx];
                auto name_str = nkl_getTokenStr(name_token, src.text);
                auto name = nk_s2atom(name_str);

                DEFINE(type_v, compileNode(ctx, type_idx));

                if (type_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(type_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                auto type = nklval_as(nkltype_t, getValueFromInfo(type_v));

                nkda_append(&fields, NklField{name, type});
            }

            auto struct_t = nkl_get_struct(nkl, {NK_SLICE_INIT(fields)});

            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = struct_t;

            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_var: {
            auto name_idx = get_next_child(next_idx);
            auto type_idx = get_next_child(next_idx);

            auto const &name_n = src.nodes.data[name_idx];
            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            DEFINE(type_v, compileNode(ctx, type_idx));

            if (type_v.type->tclass != NklType_Typeref) {
                // TODO: Improve error message
                return error(ctx, "type expected");
            }

            if (!isValueKnown(type_v)) {
                // TODO: Improve error message
                return error(ctx, "value is not known");
            }

            auto type = nklval_as(nkltype_t, getValueFromInfo(type_v));

            CHECK(defineLocal(ctx, name, type));

            // TODO: Zero init the var

            return {{.ref = {true, type}}, type, ValueKind_Ref};
        }

        case n_void: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_void(nkl);
            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst{pvalue}}, type_t, ValueKind_Const};
        }

        case n_while: {
            auto cond_idx = get_next_child(next_idx);
            auto body_idx = get_next_child(next_idx);

            printf("loop:\n");

            DEFINE(cond, compileNode(ctx, cond_idx));
            asRef(cond);

            if (cond.type->tclass != NklType_Bool) {
                // TODO: Improve error message
                return error(ctx, "bool expected");
            }

            printf("jmpz endloop\n");

            CHECK(compileStmt(ctx, body_idx));

            printf("jmp loop\n");
            printf("endloop:\n");

            break;
        }

        default: {
            return error(ctx, "unknown AST node #%s", nk_atom2cs(node.id));
        }
    }

    return ValueInfo{};
}

static Void compileStmt(Context &ctx, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    DEFINE(val, compileNode(ctx, node_idx));
    auto ref = asRef(val);
    if (ref.tmp_nonempty && ref.type->id != nkl_get_void(ctx.m->c->nkl)->id) {
        // TODO: Inspect ref instead of value?
        NKSB_FIXED_BUFFER(sb, 1024);
        nkl_type_inspect(ref.type, nksb_getStream(&sb));
        NK_LOG_DBG("Value ignored: <TODO: Inspect value>:" NKS_FMT, NKS_ARG(sb));
    }

    return {};
}

static bool compileAst(Context &ctx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (ctx.src->nodes.size) {
        compileStmt(ctx, 0);
    }

    return nkl_getErrorCount() == 0;
}

bool nkl_compileFile(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;

    nkl_errorStateEquip(&c->errors);
    defer {
        nkl_errorStateUnequip();
    };

    DEFINE(file_ctx, importFile(c, filename, &c->perm_arena, &c->temp_arenas[0], nullptr));

    // TODO: Merge `file_ctx->ctx->m` into `m`

    return true;
}
