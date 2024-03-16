#include "nkl/core/compiler.h"

#include "nkl/common/ast.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/string.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

} // namespace

struct NklCompiler_T {
    NkArena arena;
};

struct NklModule_T {
    NklCompiler c;
};

NklCompiler nkl_createCompiler(NklTargetTriple target) {
    NkArena arena{};
    auto alloc = nk_arena_getAllocator(&arena);
    return new (nk_allocT<NklCompiler_T>(alloc)) NklCompiler_T{
        .arena = arena,
    };
}

void nkl_freeCompiler(NklCompiler c) {
    auto arena = c->arena;
    nk_arena_free(&arena);
}

NklModule nkl_createModule(NklCompiler c) {
    auto alloc = nk_arena_getAllocator(&c->arena);
    return new (nk_allocT<NklModule_T>(alloc)) NklModule_T{
        .c = c,
    };
}

void nkl_writeModule(NklModule m, NkString filename) {
    NK_LOG_TRC("%s", __func__);
}

static void compileNode(NklModule m, NklSource src, NklAstNode const &node) {
    NK_LOG_TRC("%s", __func__);

    NK_LOG_DBG("Compiling #%s", nk_atom2cs(node.id));

    auto idx = &node - src.nodes.data + 1;

    switch (node.id) {
    case n_list: {
        for (size_t i = 0; i < node.arity; i++, idx = nkl_ast_nextChild(src.nodes, idx)) {
            auto const &child_node = src.nodes.data[idx];
            compileNode(m, src, child_node);
        }
        break;
    }

    case n_proc: {
        if (!node.arity) {
            NK_LOG_ERR("invalid node");
            return;
        }

        auto const &info = src.nodes.data[idx];
        idx = nkl_ast_nextChild(src.nodes, idx);
        compileNode(m, src, info);

        auto const &name = src.nodes.data[idx];
        idx = nkl_ast_nextChild(src.nodes, idx);
        compileNode(m, src, name);

        auto const &params = src.nodes.data[idx];
        idx = nkl_ast_nextChild(src.nodes, idx);
        compileNode(m, src, params);

        auto const &return_type = src.nodes.data[idx];
        idx = nkl_ast_nextChild(src.nodes, idx);
        compileNode(m, src, return_type);

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

    default:
        NK_LOG_ERR("unknown node #%s", nk_atom2cs(node.id));
        break;
    }
}

void nkl_compile(NklModule m, NklSource src) {
    NK_LOG_TRC("%s", __func__);

    if (src.nodes.size) {
        auto const &root = src.nodes.data[0];
        compileNode(m, src, root);
    }
}