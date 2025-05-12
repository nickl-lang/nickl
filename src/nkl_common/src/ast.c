#include "nkl/common/ast.h"

#include "nkl/common/token.h"
#include "ntk/string.h"

static void inspectNode(u32 idx, NklSource src, NkStream out, u32 indent) {
    nk_printf(out, "\n%5u ", idx);

    for (u32 i = 0; i < indent; i++) {
        nk_printf(out, "|  ");
    }

    if (idx >= src.nodes.size) {
        nk_printf(out, "<invalid>\n");
        return;
    }

    NklAstNode const *node = &src.nodes.data[idx++];

    if (node->id) {
        NkString const node_name = nk_atom2s(node->id);
        nk_printf(out, NKS_FMT, NKS_ARG(node_name));
    } else {
        nk_printf(out, "(null)");
    }

    if (node->token_idx < src.tokens.size) {
        NklToken const *token = &src.tokens.data[node->token_idx];
        NkString const token_text = nkl_getTokenStr(token, src.text);
        nk_printf(out, " \"");
        nks_escape(out, token_text);
        nk_printf(out, "\"");
    } else {
        nk_printf(out, " \"<invalid>\"");
    }

    for (u32 i = 0; i < node->arity; i++, idx = nkl_ast_nextChild(src.nodes, idx)) {
        inspectNode(idx, src, out, indent + 1);
    }
}

void nkl_ast_inspect(NklSource src, NkStream out) {
    if (src.nodes.size) {
        inspectNode(0, src, out, 0);
    }
}
