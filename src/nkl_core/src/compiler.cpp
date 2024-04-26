#include "nkl/core/compiler.h"

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/hash_tree.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

} // namespace

struct NklCompiler_T {
    NkArena arena;
};

// TODO: Move list macros
#define nk_list_push_n(list, item, next) ((item)->next = (list), (list) = (item))
#define nk_list_push(list, item) nk_list_push_n(list, item, next)

#define nk_list_pop_n(list, next) ((list) = (list)->next)
#define nk_list_pop(list) nk_list_pop_n(list, next)

enum EDeclKind {
    Decl_Undefined,

    Decl_Local,
};

struct Decl {
    union {
        struct {
            nkltype_t type;
        } local;
    } as;
    EDeclKind kind;
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

struct ValueInfo {};

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

static Decl &resolve(NklModule m, NkAtom name) {
    NK_LOG_DBG("Resolving name: name=`" NKS_FMT "` scope=%p", NKS_ARG(nk_atom2s(name)), (void *)m->scope_stack);

    for (auto scope = m->scope_stack; scope; scope = scope->next) {
        auto found = DeclTree_find(&scope->locals, name);
        if (found) {
            return found->val;
        }
    }
    static Decl s_undefined_decl{{}, Decl_Undefined};
    return s_undefined_decl;
}

static void defineLocal(NklModule m, NkAtom name, nkltype_t type) {
    makeDecl(m, name) = {{.local{.type = type}}, Decl_Local};
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

static void compileNode(NklModule m, NklSource src, usize node_idx) {
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
            auto const &name_n = src.nodes.data[name_idx];

            auto value_idx = get_next_child();
            compileNode(m, src, value_idx);

            NkAtom name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));
            // TODO: Get var type
            nkltype_t type = nullptr;

            defineLocal(m, name, type);

            break;
        }

        case n_id: {
            auto name_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkAtom name_atom = nk_s2atom(name_str);

            auto &decl = resolve(m, name_atom);

            if (decl.kind == Decl_Undefined) {
                // TODO: Report error properly
                NK_LOG_ERR("`" NKS_FMT "` is not defined", NKS_ARG(name_str));
                return;
            } else {
                // TODO: Handle resolved id
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
                compileNode(m, src, get_next_child());
            }
            break;
        }

        case n_proc: {
            if (!node.arity) {
                NK_LOG_ERR("invalid node");
                return;
            }

            auto info_idx = get_next_child();
            auto name_idx = get_next_child();
            auto params_idx = get_next_child();
            auto return_type_idx = get_next_child();

            compileNode(m, src, info_idx);
            compileNode(m, src, name_idx);
            compileNode(m, src, params_idx);
            compileNode(m, src, return_type_idx);

            auto const &info = src.nodes.data[info_idx];

            if (info.id == n_extern) {
                NK_LOG_ERR("extern proc compilation is not implemented");
                return;
            } else if (info.id == n_pub) {
                NK_LOG_ERR("pub proc compilation is not implemented");
                return;
            } else {
                NK_LOG_ERR("invalid node");
                return;
            }

            break;
        }

        case n_scope: {
            pushScope(m);
            defer {
                popScope(m);
            };

            auto child_idx = get_next_child();
            compileNode(m, src, child_idx);

            return;
        }

        default:
            NK_LOG_ERR("unknown node #%s", nk_atom2cs(node.id));
            break;
    }
}

void nkl_compile(NklModule m, NklSource src) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    pushScope(m);
    defer {
        popScope(m);
    };

    if (src.nodes.size) {
        compileNode(m, src, 0);
    }
}
