#include "nkl/lang/ast.h"

#include <cstring>

#include "ast_impl.h"
#include "nkl/lang/token.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

char const *s_nkl_ast_node_names[] = {
#define X(ID) #ID,
#include "nodes.inl"
};

struct NklAst_T {
    NkArena arena{};
};

namespace {

void _inspect(NklAstNodeDynArray nodes, NkStringBuilder *sb, usize depth = 1) {
    auto const _newline = [&]() {
        nksb_printf(sb, "\n%*s", (int)(depth * 2), "");
    };

    if (nodes.size > 1) {
        _newline();
        nksb_printf(sb, "list:");
        depth += 1;
    }

    for (usize node_i = 0; node_i < nodes.size; node_i++) {
        auto const node = &nodes.data[node_i];

        if (!node) {
            _newline();
            nksb_printf(sb, "(null)");
            return;
        }

        if (node->id) {
            _newline();
            auto id_str = nk_atom2s(node->id);
            nksb_printf(sb, "#" NKS_FMT, NKS_ARG(id_str));
        } else {
            _newline();
            nksb_printf(sb, "(null)");
        }

        bool print_text = true;

        for (usize arg_i = 0; arg_i < 3; arg_i++) {
            auto const &arg = node->args[arg_i];
            if (arg.size) {
                print_text = false;
                _inspect(arg, sb, depth + 1);
            }
        }

        if (node->token && print_text) {
            nksb_printf(sb, " \"");
            nks_escape(nksb_getStream(sb), node->token->text);
            nksb_printf(sb, "\"");
        }
    }
}

} // namespace

NK_EXPORT void nkl_ast_init() {
#define X(ID) nk_atom_define(NK_CAT(n_, ID), nk_cs2s(#ID));
#include "nodes.inl"
}

NklAst nkl_ast_create() {
    return new (nk_alloc(nk_default_allocator, sizeof(NklAst_T))) NklAst_T{};
}

void nkl_ast_free(NklAst ast) {
    nk_arena_free(&ast->arena);
    ast->~NklAst_T();
    nk_free(nk_default_allocator, ast, sizeof(*ast));
}

NklAstNode_T nkl_makeNode0(NkAtom id, NklTokenRef token) {
    return {.args{{}, {}, {}}, .token = token, .id = id};
}

NklAstNode_T nkl_makeNode1(NkAtom id, NklTokenRef token, NklAstNodeDynArray arg0) {
    return {.args{arg0, {}, {}}, .token = token, .id = id};
}

NklAstNode_T nkl_makeNode2(NkAtom id, NklTokenRef token, NklAstNodeDynArray arg0, NklAstNodeDynArray arg1) {
    return {.args{arg0, arg1, {}}, .token = token, .id = id};
}

NklAstNode_T nkl_makeNode3(
    NkAtom id,
    NklTokenRef token,
    NklAstNodeDynArray arg0,
    NklAstNodeDynArray arg1,
    NklAstNodeDynArray arg2) {
    return {.args{arg0, arg1, arg2}, .token = token, .id = id};
}

NklAstNodeDynArray nkl_pushNode(NklAst ast, NklAstNode_T node) {
    return {new (nk_arena_allocT<NklAstNode_T>(&ast->arena)) NklAstNode_T{node}, 1};
}

NklAstNodeDynArray nkl_pushNodeAr(NklAst ast, NklAstNodeDynArray ar) {
    auto new_ar = new (nk_arena_allocT<NklAstNode_T>(&ast->arena, ar.size)) NklAstNode_T[ar.size];
    std::memcpy(new_ar, ar.data, sizeof(NklAstNode_T) * ar.size);
    return {new_ar, ar.size};
}

void nkl_inspectNode(NklAstNode root, NkStringBuilder *sb) {
    _inspect({root, 1}, sb);
}
