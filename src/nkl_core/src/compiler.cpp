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
    DeclKind_ComptimeUnresolved,
    DeclKind_Local,
    DeclKind_Module,
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
            NklSource const *src;
            usize node_idx;
        } comptime_unresolved;
        struct {
            nkltype_t type;
        } local;
        struct {
            Scope *scope;
            nklval_t proc;
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

struct Context {
    NklModule m;
    Scope *scope_stack;
};

struct FileContext {
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

enum ValueKind {
    ValueKind_Void,

    ValueKind_Const,
    ValueKind_Decl,
    ValueKind_Instr,
};

struct ValueInfo {
    union {
        void *cnst;
        Decl *decl;
        struct {
            int dummy;
        } instr;
    } as;
    nkltype_t type;
    ValueKind kind;
};

struct NklModule_T {
    NklCompiler c;
};

static ValueInfo compileNode(Context &ctx, NklSource const &src, usize node_idx);
static Void compileStmt(Context &ctx, NklSource const &src, usize node_idx);

struct ProcCompilationResult {
    ValueInfo proc_val;
    Scope *proc_scope;
};
static ProcCompilationResult compileProc(
    Context &ctx,
    NklSource const &src,
    usize node_idx,
    NkArena *main_arena,
    NkArena *temp_arena);

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

static void defineComptimeUnresolved(Context &ctx, NkAtom name, NklSource const *src, usize node_idx) {
    makeDecl(ctx, name) = {
        {.comptime_unresolved{.ctx = &ctx, .src = src, .node_idx = node_idx}}, DeclKind_ComptimeUnresolved};
}

static void defineLocal(Context &ctx, NkAtom name, nkltype_t type) {
    makeDecl(ctx, name) = {{.local{.type = type}}, DeclKind_Local};
}

static void defineParam(Context &ctx, NkAtom name, nkltype_t type) {
    makeDecl(ctx, name) = {{.param{.type = type}}, DeclKind_Param};
}

static Decl &resolve(Context &ctx, NkAtom name) {
    NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)ctx.scope_stack);

    for (auto scope = ctx.scope_stack; scope; scope = scope->next) {
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
           (val.kind == ValueKind_Const &&
            (val.as.decl->kind == DeclKind_Comptime || val.as.decl->kind == DeclKind_Module));
}

static nklval_t getValueFromInfo(ValueInfo const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");

    switch (val.kind) {
        case ValueKind_Void:
            return {nullptr, val.type};
        case ValueKind_Const:
            return {val.as.cnst, val.type};
        case ValueKind_Decl: {
            switch (val.as.decl->kind) {
                case DeclKind_Comptime:
                    return val.as.decl->as.comptime.val;
                case DeclKind_Module:
                    return val.as.decl->as.module.proc;
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
        }
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

static ValueInfo resolveComptime(Decl &decl) {
    nk_assert(decl.kind == DeclKind_ComptimeUnresolved);

    auto &ctx = *decl.as.comptime_unresolved.ctx;
    auto const &src = *decl.as.comptime_unresolved.src;
    auto node_idx = decl.as.comptime_unresolved.node_idx;

    NK_LOG_DBG("Resolving comptime const: node %zu file=`%s`", node_idx, nk_atom2cs(src.file));

    auto const &node = &src.nodes.data[node_idx];
    if (node->id == n_proc) {
        // TODO: Assuming public visibility
        DEFINE(res, compileProc(ctx, src, node_idx, ctx.scope_stack->main_arena, ctx.scope_stack->temp_arena));
        decl.as.module.proc = getValueFromInfo(res.proc_val);
        decl.as.module.scope = res.proc_scope;
        decl.kind = DeclKind_Module;
    } else {
        DEFINE(val, compileNode(ctx, src, node_idx));

        if (!isValueKnown(val)) {
            auto node = &src.nodes.data[node_idx];
            auto token = &src.tokens.data[node->token_idx];
            // TODO: Improve error message
            return nkl_reportError(src.file, token, "value is not known"), ValueInfo{};
        }

        decl.as.comptime.val = getValueFromInfo(val);
        decl.kind = DeclKind_Comptime;
    }

    return {{.decl = &decl}, decl.as.comptime.val.type, ValueKind_Decl};
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
            return {{.decl = &decl}, decl.as.module.proc.type, ValueKind_Decl};
        case DeclKind_Param:
            return {{.decl = &decl}, decl.as.local.type, ValueKind_Decl};
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

static ProcCompilationResult compileProc(
    Context &ctx,
    NklSource const &src,
    usize node_idx,
    NkArena *main_arena,
    NkArena *temp_arena) {
    auto m = ctx.m;
    auto c = m->c;
    auto nkl = c->nkl;

    auto idx = node_idx + 1;

    // TODO: Bolerplate `get_next_child`
    auto get_next_child = [&src](usize &next_child_idx) {
        auto child_idx = next_child_idx;
        next_child_idx = nkl_ast_nextChild(src.nodes, next_child_idx);
        return child_idx;
    };

    auto params_idx = get_next_child(idx);
    auto ret_t_idx = get_next_child(idx);
    auto body_idx = get_next_child(idx);

    auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

    NkDynArray(NkAtom) param_names{NKDA_INIT(temp_alloc)};
    NkDynArray(nkltype_t) param_types{NKDA_INIT(temp_alloc)};

    auto &params_n = src.nodes.data[params_idx];
    auto next_param_idx = params_idx + 1;
    for (size_t i = 0; i < params_n.arity; i++) {
        auto param_idx = get_next_child(next_param_idx) + 1;

        auto name_idx = get_next_child(param_idx);
        auto type_idx = get_next_child(param_idx);

        auto name_n = &src.nodes.data[name_idx];
        auto name_token = &src.tokens.data[name_n->token_idx];
        auto name_str = nkl_getTokenStr(name_token, src.text);
        auto name = nk_s2atom(name_str);

        DEFINE(type_v, compileNode(ctx, src, type_idx));

        if (type_v.type->tclass != NklType_Typeref) {
            auto type_n = &src.nodes.data[type_idx];
            auto token = &src.tokens.data[type_n->token_idx];
            // TODO: Improve error message
            return nkl_reportError(src.file, token, "type expected"), ProcCompilationResult{};
        }

        if (!isValueKnown(type_v)) {
            auto type_n = &src.nodes.data[type_idx];
            auto token = &src.tokens.data[type_n->token_idx];
            // TODO: Improve error message
            return nkl_reportError(src.file, token, "value is not known"), ProcCompilationResult{};
        }

        auto type = nklval_as(nkltype_t, getValueFromInfo(type_v));

        nkda_append(&param_names, name);
        nkda_append(&param_types, type);
    }

    DEFINE(ret_t_v, compileNode(ctx, src, ret_t_idx));

    // TODO: Boilerplate in type checking
    if (ret_t_v.type->tclass != NklType_Typeref) {
        auto type_n = &src.nodes.data[ret_t_idx];
        auto token = &src.tokens.data[type_n->token_idx];
        // TODO: Improve error message
        return nkl_reportError(src.file, token, "type expected"), ProcCompilationResult{};
    }

    if (!isValueKnown(ret_t_v)) {
        auto type_n = &src.nodes.data[ret_t_idx];
        auto token = &src.tokens.data[type_n->token_idx];
        // TODO: Improve error message
        return nkl_reportError(src.file, token, "value is not known"), ProcCompilationResult{};
    }

    auto ret_t = nklval_as(nkltype_t, getValueFromInfo(ret_t_v));

    pushScope(ctx, main_arena, temp_arena);
    defer {
        popScope(ctx);
    };

    auto proc_scope = ctx.scope_stack;

    for (usize i = 0; i < params_n.arity; i++) {
        CHECK(defineParam(ctx, param_names.data[i], param_types.data[i]));
    }

    CHECK(compileStmt(ctx, src, body_idx));

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

    return {
        // TODO: Returning null as proc value
        .proc_val = {{.cnst = nullptr}, proc_t, ValueKind_Const},
        .proc_scope = proc_scope,
    };
}

static ValueInfo compileNode(Context &ctx, NklSource const &src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto m = ctx.m;
    auto c = m->c;
    auto nkl = c->nkl;

    auto temp_frame = nk_arena_grab(ctx.scope_stack->temp_arena);
    defer {
        nk_arena_popFrame(ctx.scope_stack->temp_arena, temp_frame);
    };

    auto const &node = src.nodes.data[node_idx];

    NK_LOG_DBG("Compiling node %zu #%s", node_idx, nk_atom2cs(node.id));

    auto idx = node_idx + 1;

    auto get_next_child = [&src](usize &next_child_idx) {
        auto child_idx = next_child_idx;
        next_child_idx = nkl_ast_nextChild(src.nodes, next_child_idx);
        return child_idx;
    };

    switch (node.id) {
        case n_add: {
            auto lhs_idx = get_next_child(idx);
            auto rhs_idx = get_next_child(idx);

            DEFINE(lhs, compileNode(ctx, src, lhs_idx));
            DEFINE(rhs, compileNode(ctx, src, rhs_idx));
            // TODO: Assuming equal and correct types in add
            NK_LOG_INF("IR: add {lhs}, {rhs}");
            return {{.instr{}}, lhs.type, ValueKind_Instr};
        }

        case n_define: {
            auto name_idx = get_next_child(idx);
            auto value_idx = get_next_child(idx);

            auto const &name_n = src.nodes.data[name_idx];
            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            CHECK(defineComptimeUnresolved(ctx, name, &src, value_idx));

            return ValueInfo{};
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
            return ValueInfo{{.cnst = (void *)str.data}, str_t, ValueKind_Const};
        }

        case n_i32: {
            auto pvalue = nk_arena_allocT<nkltype_t>(&c->perm_arena);
            *pvalue = nkl_get_numeric(nkl, Int32);

            // TODO: Hardcoded word size
            auto type_t = nkl_get_typeref(nkl, 8);
            return ValueInfo{{.cnst = pvalue}, type_t, ValueKind_Const};
        }

        case n_id: {
            auto token = &src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            auto name = nk_s2atom(name_str);

            auto &decl = resolve(ctx, name);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return nkl_reportError(src.file, token, "`" NKS_FMT "` is not defined", NKS_ARG(name_str)), ValueInfo{};
            } else {
                return resolveDecl(decl);
            }
        }

        case n_int: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            auto pvalue = nk_arena_allocT<int64_t>(&c->perm_arena);
            // TODO: Replace sscanf in compiler
            int res = sscanf(token_str.data, "%" SCNi64, pvalue);
            nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
            return ValueInfo{{.cnst = pvalue}, nkl_get_numeric(nkl, Int64), ValueKind_Const};
        }

        case n_list: {
            for (size_t i = 0; i < node.arity; i++) {
                CHECK(compileNode(ctx, src, get_next_child(idx)));
            }
            break;
        }

        case n_proc: {
            // TODO: Not choosing arenas based on proc visibility
            DEFINE(
                res,
                compileProc(
                    ctx, src, node_idx, ctx.scope_stack->temp_arena, getNextTempArena(c, ctx.scope_stack->temp_arena)));
            return res.proc_val;
        }

        case n_return: {
            // TODO: Typecheck the returned value
            if (node.arity) {
                auto arg_idx = get_next_child(idx);
                DEFINE(arg, compileNode(ctx, src, arg_idx));
                NK_LOG_INF("IR: return {arg}");
            } else {
                NK_LOG_INF("IR: return");
            }
            return {};
        }

        case n_scope: {
            pushScope(ctx, ctx.scope_stack->temp_arena, getNextTempArena(c, ctx.scope_stack->temp_arena));
            defer {
                popScope(ctx);
            };

            auto child_idx = get_next_child(idx);
            CHECK(compileNode(ctx, src, child_idx));

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
            return ValueInfo{{.cnst = (void *)str.data}, str_t, ValueKind_Const};
        }

        default: {
            auto token = &src.tokens.data[node.token_idx];
            return nkl_reportError(src.file, token, "unknown AST node #%s", nk_atom2cs(node.id)), ValueInfo{};
        }
    }

    return ValueInfo{};
}

static Void compileStmt(Context &ctx, NklSource const &src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    DEFINE(val, compileNode(ctx, src, node_idx));
    if (val.kind != ValueKind_Void) {
        NK_LOG_DBG("Value ignored: <TODO: Inspect value>");
    }

    return {};
}

static bool compileAst(Context &ctx, NklSource const &src) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (src.nodes.size) {
        compileStmt(ctx, src, 0);
    }

    return nkl_getErrorCount() == 0;
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

bool nkl_compileFile(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;
    auto nkl = c->nkl;

    nkl_errorStateEquip(&c->errors);
    defer {
        nkl_errorStateUnequip();
    };

    auto file = getFileId(filename);

    auto &src = *nkl_getSource(nkl, file);

    if (nkl_getErrorCount()) {
        return false;
    }

    auto &file_ctx = getContextForFile(c, file);
    if (file_ctx.ctx) {
        nkl_reportError(NK_ATOM_INVALID, nullptr, "file `%s` has already been compiled", nk_atom2cs(file));
        return false;
    }

    file_ctx.ctx = nk_arena_allocT<Context>(&c->perm_arena);
    file_ctx.ctx->m = nkl_createModule(c);

    pushScope(*file_ctx.ctx, &c->perm_arena, &c->temp_arenas[0]);

    if (compileAst(*file_ctx.ctx, src)) {
        // TODO: Merge `file_ctx.module` into `m`
        return true;
    }

    return false;
}
