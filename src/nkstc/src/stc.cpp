#include "stc.h"

#include "lexer.h"
#include "nkl/core/compiler.h"
#include "nkl/core/nickl.h"
#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

NK_LOG_USE_SCOPE(nkstc);

NklTokenArray lexer_proc(void *data, NkString text) {
    return nkst_lex((NkArena *)data, text);
}

NklAstNodeArray parser_proc(void *data, NkString text, NklTokenArray tokens) {
    return nkst_parse((NkArena *)data, text, tokens);
}

} // namespace

int nkst_compile(NkString in_file) {
    NK_LOG_TRC("%s", __func__);

    NkArena file_arena{};
    defer {
        nk_arena_free(&file_arena);
    };

    NklLexer lexer{.data = &file_arena, .proc = lexer_proc};
    NklParser parser{.data = &file_arena, .proc = parser_proc};

    NklState nkl{};
    nkl_state_init(&nkl, lexer, parser);
    defer {
        nkl_state_free(&nkl);
    };

    auto c = nkl_createCompiler(&nkl, {});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    if (!nkl_compileFile(m, in_file)) {
        return 1;
    }

    nkl_writeModule(m, nk_cs2s("a.out"));

    return 0;
}
