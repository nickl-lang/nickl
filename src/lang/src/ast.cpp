#include "nkl/lang/ast.h"

#include <cstring>
#include <deque>
#include <new>

#include "ast_impl.h"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/string_builder.h"

char const *s_nkl_ast_node_names[] = {
#define X(ID) #ID,
#include "nodes.inl"
};

struct NklAst_T {
    NkAllocator *arena;
};

namespace {

void _inspect(NklAstNodeArray nodes, NkStringBuilder sb, size_t depth = 1) {
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
            nksb_printf(sb, " \"");
            nksb_str_escape(sb, node->token->text);
            nksb_printf(sb, "\"");
        }
    }
}

} // namespace

NklAst nkl_ast_create() {
    // TODO Defining ast node ids every time
#define X(ID) nkid_define(CAT(n_, ID), cs2s(#ID));
#include "nodes.inl"

    return new (nk_allocate(nk_default_allocator, sizeof(NklAst_T))) NklAst_T{
        .arena = nk_create_arena(),
    };
}

void nkl_ast_free(NklAst ast) {
    nk_free_arena(ast->arena);
    ast->~NklAst_T();
    nk_free(nk_default_allocator, ast);
}

void nkl_ast_inspect(NklAstNode root, NkStringBuilder sb) {
    _inspect({root, 1}, sb);
}

NklAstNode nkl_ast_pushNode(NklAst ast, NklAstNode_T node) {
    return new (nk_allocate(ast->arena, sizeof(NklAstNode_T))) NklAstNode_T{node};
}

NklAstNodeArray nkl_ast_pushNodeAr(NklAst ast, NklAstNodeArray ar) {
    auto new_ar =
        new (nk_allocate(ast->arena, sizeof(NklAstNode_T) * ar.size)) NklAstNode_T[ar.size];
    std::memcpy(new_ar, ar.data, sizeof(NklAstNode_T) * ar.size);
    return {new_ar, ar.size};
}
