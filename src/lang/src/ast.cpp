#include "nkl/lang/ast.h"

#include <cstring>
#include <deque>
#include <new>

#include "ast_impl.h"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/string_builder.h"
#include "nkl/lang/token.h"

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
        } else {
            _newline();
            nksb_printf(sb, "(null)");
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

NklAstNode_T nkl_makeNode0(char const *id, NklTokenRef token) {
    return {.args{{}, {}, {}}, .token = token, .id = cs2nkid(id)};
}

NklAstNode_T nkl_makeNode1(char const *id, NklTokenRef token, NklAstNodeArray arg0) {
    return {.args{arg0, {}, {}}, .token = token, .id = cs2nkid(id)};
}

NklAstNode_T nkl_makeNode2(
    char const *id,
    NklTokenRef token,
    NklAstNodeArray arg0,
    NklAstNodeArray arg1) {
    return {.args{arg0, arg1, {}}, .token = token, .id = cs2nkid(id)};
}

NklAstNode_T nkl_makeNode3(
    char const *id,
    NklTokenRef token,
    NklAstNodeArray arg0,
    NklAstNodeArray arg1,
    NklAstNodeArray arg2) {
    return {.args{arg0, arg1, arg2}, .token = token, .id = cs2nkid(id)};
}

NklAstNodeArray nkl_pushNode(NklAst ast, NklAstNode_T node) {
    return {new (nk_allocate(ast->arena, sizeof(NklAstNode_T))) NklAstNode_T{node}, 1};
}

NklAstNodeArray nkl_pushNodeAr(NklAst ast, NklAstNodeArray ar) {
    auto new_ar =
        new (nk_allocate(ast->arena, sizeof(NklAstNode_T) * ar.size)) NklAstNode_T[ar.size];
    std::memcpy(new_ar, ar.data, sizeof(NklAstNode_T) * ar.size);
    return {new_ar, ar.size};
}

void nkl_inspectNode(NklAstNode root, NkStringBuilder sb) {
    _inspect({root, 1}, sb);
}
