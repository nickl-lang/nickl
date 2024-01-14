#include "nkl/common/ast.h"

#include "ntk/string.h"

static void inspectNode(uint32_t idx, NklAstNodeView nodes, NklTokenView tokens, nk_stream out, uint32_t indent) {
    nk_printf(out, "\n");

    for (uint32_t i = 0; i < indent; i++) {
        nk_printf(out, "  ");
    }

    if (idx >= nodes.size) {
        nk_printf(out, "<invalid>\n");
        return;
    }

    NklAstNode const *node = &nodes.data[idx];

    if (node->id) {
        nks const node_name = nkid2s(node->id);
        nk_printf(out, "#" nks_Fmt, nks_Arg(node_name));
    } else {
        nk_printf(out, "(null)");
    }

    if (node->token_idx < tokens.size) {
        nks const token_text = tokens.data[node->token_idx].text;
        nk_printf(out, " \"");
        nks_escape(out, token_text);
        nk_printf(out, "\"");
    } else {
        nk_printf(out, " \"<invalid>\"");
    }

    for (uint32_t i = 0; i < node->arity; i++) {
        idx++;
        inspectNode(idx, nodes, tokens, out, indent + 1);
        if (idx < nodes.size) {
            NklAstNode const *child_node = &nodes.data[idx];
            idx += child_node->total_children;
        }
    }
}

void nkl_ast_inspect(NklAstNodeView nodes, NklTokenView tokens, nk_stream out) {
    if (nodes.size) {
        inspectNode(0, nodes, tokens, out, 0);
    }
}
