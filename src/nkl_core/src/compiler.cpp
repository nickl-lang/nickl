#include "nkl/core/compiler.h"

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/hash_tree.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/os/path.h"
#include "ntk/profiler.h"
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
    DeclKind_Module,
    DeclKind_Var,
};

struct Scope;

struct Decl {
    union {
        struct {
            nklval_t val;
        } comptime;
        struct {
            NklSource const *src;
            usize node_idx;
        } comptime_unresolved;
        struct {
            Scope *scope;
            nklval_t proc;
        } module;
        struct {
            nkltype_t type;
        } var;
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

    NkArena *perm_arena;
    NkArena *temp_arena;
    NkArenaFrame temp_frame;

    DeclMap locals;
};

struct Context {
    Scope *scope_stack;
};

struct FileContext {
    Context *ctx;
    NklModule module;
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
    NkArena temp_arena; // TODO: Use scrach arena?
    FileMap files;
    NklErrorState errors;
};

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
};

struct ValueInfo {
    union {
        void *cnst;
        Decl *decl;
    } as;
    nkltype_t type;
    ValueKind kind;
};

struct NklModule_T {
    NklCompiler c;
    NkArena module_arena;
};

static ValueInfo compileNode(NklModule m, Context &ctx, NklSource const &src, usize node_idx);

static void pushScope(Context &ctx, NkArena *perm_arena, NkArena *temp_arena) {
    auto scope = new (nk_arena_allocT<Scope>(perm_arena)) Scope{
        .next{},

        .perm_arena = perm_arena,
        .temp_arena = temp_arena,
        .temp_frame = nk_arena_grab(temp_arena),

        .locals{nullptr, nk_arena_getAllocator(perm_arena)},
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
    NK_LOG_DBG("Making declaration: name=`" NKS_FMT "` scope=%p", NKS_ARG(nk_atom2s(name)), (void *)ctx.scope_stack);
    // TODO: Check for name conflict
    auto kv = DeclMap_insert(&ctx.scope_stack->locals, {name, {}});
    return kv->val;
}

static void defineComptime(Context &ctx, NkAtom name, nklval_t val) {
    makeDecl(ctx, name) = {{.comptime{.val = val}}, DeclKind_Comptime};
}

static void defineComptimeUnresolved(Context &ctx, NkAtom name, NklSource const *src, usize node_idx) {
    makeDecl(ctx, name) = {{.comptime_unresolved{.src = src, .node_idx = node_idx}}, DeclKind_ComptimeUnresolved};
}

static void defineVar(Context &ctx, NkAtom name, nkltype_t type) {
    makeDecl(ctx, name) = {{.var{.type = type}}, DeclKind_Var};
}

static Decl &resolve(NklModule m, Context &ctx, NkAtom name) {
    NK_LOG_DBG("Resolving id: name=`" NKS_FMT "` scope=%p", NKS_ARG(nk_atom2s(name)), (void *)ctx.scope_stack);

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
        .temp_arena = {},
        .files = {},
        .errors = {},
    };
    c->files.alloc = nk_arena_getAllocator(&c->perm_arena);
    c->errors.arena = &c->perm_arena;
    return c;
}

void nkl_freeCompiler(NklCompiler c) {
    auto arena = c->perm_arena;
    nk_arena_free(&arena);
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
        .module_arena = {},
    };
}

void nkl_writeModule(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);
}

bool isValueKnown(ValueInfo const &val) {
    return val.kind == ValueKind_Void || val.kind == ValueKind_Const ||
           (val.kind == ValueKind_Const &&
            (val.as.decl->kind == DeclKind_Comptime || val.as.decl->kind == DeclKind_ComptimeUnresolved ||
             val.as.decl->kind == DeclKind_Module));
}

nklval_t getValueFromInfo(NklState nkl, ValueInfo const &val) {
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
    }
}

ValueInfo resolveComptime(NklModule m, Context &ctx, Decl &decl) {
    nk_assert(decl.kind == DeclKind_ComptimeUnresolved);

    auto &ur_decl = decl.as.comptime_unresolved;

    NK_LOG_DBG("Resolving comptime: node#%zu @ file=`%s`", ur_decl.node_idx, nk_atom2cs(ur_decl.src->file));

    DEFINE(val, compileNode(m, ctx, *ur_decl.src, ur_decl.node_idx));

    auto c = m->c;
    auto nkl = c->nkl;

    if (!isValueKnown(val)) {
        auto node = &ur_decl.src->nodes.data[ur_decl.node_idx];
        auto token = &ur_decl.src->tokens.data[node->token_idx];
        // TODO: Improve error description
        return nkl_reportError(ur_decl.src->file, token, "value is not known"), ValueInfo{};
    }

    decl.as.comptime.val = getValueFromInfo(nkl, val);
    decl.kind = DeclKind_Comptime;

    return {{.decl = &decl}, decl.as.comptime.val.type, ValueKind_Decl};
}

ValueInfo resolveDecl(NklModule m, Context &ctx, Decl &decl) {
    switch (decl.kind) {
        case DeclKind_Comptime:
            return {{.decl = &decl}, decl.as.comptime.val.type, ValueKind_Decl};
        case DeclKind_ComptimeUnresolved:
            return resolveComptime(m, ctx, decl);
        case DeclKind_Module:
            return {{.decl = &decl}, decl.as.module.proc.type, ValueKind_Decl};
        case DeclKind_Var:
            return {{.decl = &decl}, decl.as.var.type, ValueKind_Decl};
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

static ValueInfo compileNode(NklModule m, Context &ctx, NklSource const &src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;
    auto nkl = c->nkl;

    auto temp_frame = nk_arena_grab(&c->temp_arena);
    defer {
        nk_arena_popFrame(&c->temp_arena, temp_frame);
    };

    auto const &node = src.nodes.data[node_idx];

    NK_LOG_DBG("Compiling #%s", nk_atom2cs(node.id));

    auto idx = node_idx + 1;

    auto get_next_child = [&]() {
        auto child_idx = idx;
        idx = nkl_ast_nextChild(src.nodes, idx);
        return child_idx;
    };

    switch (node.id) {
        case n_define: {
            auto name_idx = get_next_child();
            auto value_idx = get_next_child();

            auto const &name_n = src.nodes.data[name_idx];
            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            CHECK(defineComptimeUnresolved(ctx, name, &src, value_idx));

            break;
        }

        case n_escaped_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(&c->temp_arena))};
            nks_unescape(nksb_getStream(&sb), text);
            NkString const unescaped_text{NKS_INIT(sb)};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, unescaped_text.size + 1);
            auto str_t = nkl_get_ptr(nkl, 8, ar_t, true);

            auto str = nks_copyNt(nk_arena_getAllocator(&m->module_arena), unescaped_text);
            return ValueInfo{{.cnst = (void *)str.data}, str_t, ValueKind_Const};
        }

        case n_id: {
            auto token = &src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            NkAtom name_atom = nk_s2atom(name_str);

            auto &decl = resolve(m, ctx, name_atom);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return nkl_reportError(src.file, token, "`" NKS_FMT "` is not defined", NKS_ARG(name_str)), ValueInfo{};
            } else {
                return resolveDecl(m, ctx, decl);
            }

            break;
        }

        case n_int: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            auto pvalue = nk_arena_allocT<int64_t>(&m->module_arena);
            // TODO: Replace sscanf in compiler
            int res = sscanf(token_str.data, "%" SCNi64, pvalue);
            nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
            return ValueInfo{{.cnst = pvalue}, nkl_get_numeric(nkl, Int64), ValueKind_Const};
        }

        case n_list: {
            for (size_t i = 0; i < node.arity; i++) {
                CHECK(compileNode(m, ctx, src, get_next_child()));
            }
            break;
        }

        case n_proc: {
            if (!node.arity) {
                NK_LOG_ERR("invalid node");
                return ValueInfo{};
            }

            auto info_idx = get_next_child();
            auto name_idx = get_next_child();
            auto params_idx = get_next_child();
            auto return_type_idx = get_next_child();

            CHECK(compileNode(m, ctx, src, info_idx));
            CHECK(compileNode(m, ctx, src, name_idx));
            CHECK(compileNode(m, ctx, src, params_idx));
            CHECK(compileNode(m, ctx, src, return_type_idx));

            auto const &info = src.nodes.data[info_idx];

            if (info.id == n_extern) {
                NK_LOG_ERR("extern proc compilation is not implemented");
                return ValueInfo{};
            } else if (info.id == n_pub) {
                NK_LOG_ERR("pub proc compilation is not implemented");
                return ValueInfo{};
            } else {
                NK_LOG_ERR("invalid node");
                return ValueInfo{};
            }

            break;
        }

        case n_scope: {
            pushScope(ctx, ctx.scope_stack->temp_arena, ctx.scope_stack->perm_arena);
            defer {
                popScope(ctx);
            };

            auto child_idx = get_next_child();
            CHECK(compileNode(m, ctx, src, child_idx));

            return ValueInfo{};
        }

        case n_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, text.size + 1);
            auto str_t = nkl_get_ptr(nkl, 8, ar_t, true);

            auto str = nks_copyNt(nk_arena_getAllocator(&m->module_arena), text);
            return ValueInfo{{.cnst = (void *)str.data}, str_t, ValueKind_Const};
        }

        default:
            NK_LOG_ERR("unknown node #%s", nk_atom2cs(node.id));
            break;
    }

    return ValueInfo{};
}

static Void compileStmt(NklModule m, Context &ctx, NklSource const &src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    DEFINE(val, compileNode(m, ctx, src, node_idx));
    if (val.kind != ValueKind_Void) {
        NK_LOG_DBG("Value ignored: <TODO: Inspect value>");
    }

    return {};
}

static bool compileAst(NklModule m, Context &ctx, NklSource const &src) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (src.nodes.size) {
        compileStmt(m, ctx, src, 0);
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
        nkl_reportError(
            NK_ATOM_INVALID, nullptr, "file `" NKS_FMT "` has already been compiled", NKS_ARG(nk_atom2s(file)));
        return false;
    }

    file_ctx = {
        .ctx = nk_arena_allocT<Context>(&c->perm_arena),
        .module = nkl_createModule(c),
    };

    pushScope(*file_ctx.ctx, &c->perm_arena, &c->temp_arena);

    return compileAst(file_ctx.module, *file_ctx.ctx, src);
}
