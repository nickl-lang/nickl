#include "nkl/common/ast.h"

static void inspectNode(uint32_t idx, NklAstNodeView nodes, NklTokenView tokens, nk_stream out, uint32_t indent) {
    for (uint32_t i = 0; i < indent; i++) {
        nk_printf(out, "  ");
    }

    auto const &node = nodes.data[idx];

    auto const node_name = nkid2s(node.id);
    nk_printf(out, nks_Fmt " \"" nks_Fmt "\"\n", nks_Arg(node_name), nks_Arg(tokens.data[node.token_idx].text));

    for (uint32_t i = 0; i < node.arity; i++) {
        auto const &child_node = nodes.data[++idx];
        inspectNode(idx, nodes, tokens, out, indent + 1);
        idx += child_node.total_children;
    }
}

void nkl_ast_inspect(NklAstNodeView nodes, NklTokenView tokens, nk_stream out) {
    if (nodes.size) {
        inspectNode(0, nodes, tokens, out, 0);
    }
}
