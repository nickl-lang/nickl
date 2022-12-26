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
    auto const _newline = [&]() {
        nksb_printf(sb, "\n%*s", depth * 2, "");
    };

    if (nodes.size > 1) {
        _newline();
        nksb_printf(sb, "list:");
        depth += 1;
    }

    for (size_t node_i = 0; node_i < nodes.size; node_i++) {
        auto const node = &nodes.data[node_i];

        if (!node) {
            _newline();
            nksb_printf(sb, "(null)");
            return;
        }

        if (node->id) {
            _newline();
            nksb_printf(sb, "#%s", nkid2cs(node->id));
        }

        bool print_text = true;

        for (size_t arg_i = 0; arg_i < 3; arg_i++) {
            auto const &arg = node->args[arg_i];
            if (arg.size) {
                print_text = false;
                _inspect(arg, sb, depth + 1);
            }
        }

        if (node->token && print_text) {
            nksb_printf(sb, " \"%.*s\"", node->token->text.size, node->token->text.data);
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
