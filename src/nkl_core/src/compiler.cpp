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
#include "ntk/profiler.h"
#include "ntk/string.h"

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

struct NklCompiler_T {
    NkArena arena;
};

enum DeclKind {
    DeclKind_Undefined,

    DeclKind_Local,
};

struct Decl {
    union {
        struct {
            nkltype_t type;
        } local;
    } as;
    DeclKind kind;
};

struct Decl_kv {
    NkAtom key;
    Decl val;
};

static NkAtom const *Decl_kv_GetKey(Decl_kv const *item) {
    return &item->key;
}
static u64 nk_atom_hash(NkAtom const key) {
    return nk_hashVal(key);
}
static bool nk_atom_equal(NkAtom const lhs, NkAtom const rhs) {
    return lhs == rhs;
}
NK_HASH_TREE_DEFINE(DeclTree, Decl_kv, NkAtom, Decl_kv_GetKey, nk_atom_hash, nk_atom_equal);

struct Scope {
    Scope *next;
    NkArenaFrame frame;
    DeclTree locals;
};

enum ValueKind {
    ValueKind_Void,

    ValueKind_Decl,
};

struct ValueInfo {
    union {
        Decl *decl;
    } as;
    nkltype_t type;
    ValueKind kind;
};

struct NklModule_T {
    NklCompiler c;
    NkArena scope_arena;
    Scope *scope_stack;
};

static void pushScope(NklModule m) {
    auto frame = nk_arena_grab(&m->scope_arena);
    auto alloc = nk_arena_getAllocator(&m->scope_arena);
    auto scope = new (nk_allocT<Scope>(alloc)) Scope{
        .next{},
        .frame = frame,
        .locals{nullptr, alloc},
    };
    nk_list_push(m->scope_stack, scope);
}

static void popScope(NklModule m) {
    nk_assert(m->scope_stack && "no current scope");
    auto frame = m->scope_stack->frame;
    nk_list_pop(m->scope_stack);
    nk_arena_popFrame(&m->scope_arena, frame);
}

static Decl &makeDecl(NklModule m, NkAtom name) {
    nk_assert(m->scope_stack && "no current scope");
    NK_LOG_DBG("Making declaration: name=`" NKS_FMT "` scope=%p", NKS_ARG(nk_atom2s(name)), (void *)m->scope_stack);
    // TODO: Check for name conflict
    auto kv = DeclTree_insert(
        &m->scope_stack->locals,
        {
            .key = name,
            .val = {},
        });
    return kv->val;
}

static void defineLocal(NklModule m, NkAtom name, nkltype_t type) {
    makeDecl(m, name) = {{.local{.type = type}}, DeclKind_Local};
}

static Decl &resolve(NklModule m, NkAtom name) {
    NK_LOG_DBG("Resolving name: name=`" NKS_FMT "` scope=%p", NKS_ARG(nk_atom2s(name)), (void *)m->scope_stack);

    for (auto scope = m->scope_stack; scope; scope = scope->next) {
        auto found = DeclTree_find(&scope->locals, name);
        if (found) {
            return found->val;
        }
    }
    static Decl s_undefined_decl{{}, DeclKind_Undefined};
    return s_undefined_decl;
}

NklCompiler nkl_createCompiler(NklTargetTriple target) {
    NkArena arena{};
    return new (nk_allocT<NklCompiler_T>(nk_arena_getAllocator(&arena))) NklCompiler_T{
        .arena = arena,
    };
}

void nkl_freeCompiler(NklCompiler c) {
    auto arena = c->arena;
    nk_arena_free(&arena);
}

NklModule nkl_createModule(NklCompiler c) {
    return new (nk_allocT<NklModule_T>(nk_arena_getAllocator(&c->arena))) NklModule_T{
        .c = c,
        .scope_arena{},
        .scope_stack{},
    };
}

void nkl_writeModule(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);
}

ValueInfo declInfo(Decl &decl) {
    switch (decl.kind) {
        case DeclKind_Local:
            return {{.decl = &decl}, decl.as.local.type, ValueKind_Decl};
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

static ValueInfo compileNode(NklModule m, NklSource src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

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

            DEFINE(val, compileNode(m, src, value_idx));
            CHECK(defineLocal(m, name, val.type));

            break;
        }

        case n_id: {
            auto token = &src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            NkAtom name_atom = nk_s2atom(name_str);

            auto &decl = resolve(m, name_atom);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return nkl_reportError(token, "`" NKS_FMT "` is not defined", NKS_ARG(name_str)), ValueInfo{};
            } else {
                return declInfo(decl);
            }

            break;
        }

        case n_int: {
            // TODO: WIP int parsing
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NK_LOG_INF("value=" NKS_FMT, NKS_ARG(token_str));
            break;
        }

        case n_list: {
            for (size_t i = 0; i < node.arity; i++) {
                CHECK(compileNode(m, src, get_next_child()));
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

            CHECK(compileNode(m, src, info_idx));
            CHECK(compileNode(m, src, name_idx));
            CHECK(compileNode(m, src, params_idx));
            CHECK(compileNode(m, src, return_type_idx));

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
            pushScope(m);
            defer {
                popScope(m);
            };

            auto child_idx = get_next_child();
            CHECK(compileNode(m, src, child_idx));

            return ValueInfo{};
        }

        default:
            NK_LOG_ERR("unknown node #%s", nk_atom2cs(node.id));
            break;
    }

    return ValueInfo{};
}

static Void compileStmt(NklModule m, NklSource src, usize node_idx) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    DEFINE(val, compileNode(m, src, node_idx));
    if (val.kind != ValueKind_Void) {
        NK_LOG_DBG("Value ignored: <TODO: Inspect value>");
    }

    return {};
}

bool nkl_compile(NklModule m, NklSource src) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    pushScope(m);
    defer {
        popScope(m);
    };

    if (src.nodes.size) {
        compileStmt(m, src, 0);
    }

    return nkl_getErrorCount() == 0;
}
