#include "nkl/common/ast.h"

#include "ntk/string.h"

static void inspectNode(uint32_t idx, NklSource src, nk_stream out, uint32_t indent) {
    nk_printf(out, "\n");

    for (uint32_t i = 0; i < indent; i++) {
        nk_printf(out, "  ");
    }

    if (idx >= src.nodes.size) {
        nk_printf(out, "<invalid>\n");
        return;
    }

    NklAstNode const *node = &src.nodes.data[idx];

    if (node->id) {
        nks const node_name = nkid2s(node->id);
        nk_printf(out, "#" nks_Fmt, nks_Arg(node_name));
    } else {
        nk_printf(out, "(null)");
    }

    if (node->token_idx < src.tokens.size) {
        NklToken const *token = &src.tokens.data[node->token_idx];
        nks const token_text = (nks){&src.text.data[token->pos], token->len};
        nk_printf(out, " \"");
        nks_escape(out, token_text);
        nk_printf(out, "\"");
    } else {
        nk_printf(out, " \"<invalid>\"");
    }

    for (uint32_t i = 0; i < node->arity; i++) {
        idx++;
        inspectNode(idx, src, out, indent + 1);
        if (idx < src.nodes.size) {
            NklAstNode const *child_node = &src.nodes.data[idx];
            idx += child_node->total_children;
        }
    }
}

void nkl_ast_inspect(NklSource src, nk_stream out) {
    if (src.nodes.size) {
        inspectNode(0, src, out, 0);
    }
}
