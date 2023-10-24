#include "parser.h"

#include "nkl/common/ast.h"
#include "ntk/file.h"
#include "ntk/logger.h"

namespace {

NK_LOG_USE_SCOPE(parser);

} // namespace

bool nkst_parse(NkStParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NklTokenView tokens) {
    NK_LOG_TRC("%s", __func__);

    // TODO Hardcoded example {
    NklAstNode nodes[] = {
        {.id = cs2nkid("add"), .token_idx = 1, .total_children = 2, .arity = 2},
        {.id = cs2nkid("int"), .token_idx = 0, .total_children = 0, .arity = 0},
        {.id = cs2nkid("int"), .token_idx = 2, .total_children = 0, .arity = 0},
    };
    NklToken example_tokens[] = {
        {.text = nk_cs2s("4"), .id = 0, .lin = 1, .col = 1},
        {.text = nk_cs2s("+"), .id = 0, .lin = 1, .col = 2},
        {.text = nk_cs2s("5"), .id = 0, .lin = 1, .col = 3},
    };
    nk_stream out = nk_file_getStream(nk_stdout());
    nkl_ast_inspect({nodes, sizeof(nodes)}, {example_tokens, sizeof(example_tokens)}, out);
    // }

    return true;
}
