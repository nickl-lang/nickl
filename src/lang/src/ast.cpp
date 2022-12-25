#include "nkl/lang/ast.h"

#include <cstring>
#include <deque>
#include <new>

#include "nk/common/allocator.h"
#include "nk/common/string_builder.h"

struct NklAst_T {
    NkAllocator *arena;
};

namespace {

void _inspect(NklNodeArray nodes, NkStringBuilder sb, size_t depth = 1) {
    auto const _indent = [&]() {
        nksb_printf(sb, "%0*c", depth * 2, ' ');
    };

    for (size_t node_i = 0; node_i < nodes.size; node_i++) {
        auto const node = &nodes.data[node_i];

        if (!node) {
            _indent();
            nksb_printf(sb, "(null)\n");
            return;
        }

        if (node->id) {
            _indent();
            nksb_printf(sb, "id: %s\n", nkid2cs(node->id));
        }

        if (node->token) {
            _indent();
            nksb_printf(sb, "text: \"%.*s\"\n", node->token->text.size, node->token->text.data);
        }

        for (size_t arg_i = 0; arg_i < 3; arg_i++) {
            _inspect(node->args[arg_i], sb, depth + 1);
        }
    }
}

} // namespace

NklAst nkl_ast_create() {
    auto ast = new (nk_allocate(nk_default_allocator, sizeof(NklAst_T))) NklAst_T{
        .arena = nk_create_arena(),
    };
    return ast;
}

void nkl_ast_free(NklAst ast) {
    nk_free_arena(ast->arena);
    ast->~NklAst_T();
    nk_free(nk_default_allocator, ast);
}

void nkl_ast_inspect(NklNode const *root, NkStringBuilder sb) {
    _inspect({root, 1}, sb);
}

NklNode const *nkl_ast_pushNode(NklAst ast, NklNode node) {
    return new (nk_allocate(ast->arena, sizeof(NklNode))) NklNode{node};
}

NklNodeArray nkl_ast_pushNodeAr(NklAst ast, NklNodeArray ar) {
    auto new_ar = new (nk_allocate(ast->arena, sizeof(NklNode) * ar.size)) NklNode[ar.size];
    std::memcpy(new_ar, ar.data, sizeof(NklNode) * ar.size);
    return {new_ar, ar.size};
}
