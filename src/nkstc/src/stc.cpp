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

NklTokenArray lexer_proc(void *, NklState nkl, NkAllocator alloc, NkString text) {
    return nkst_lex(nkl, alloc, text);
}

NklAstNodeArray parser_proc(void *, NklState nkl, NkAllocator alloc, NkString text, NklTokenArray tokens) {
    return nkst_parse(nkl, alloc, text, tokens);
}

} // namespace

int nkst_compile(NkString in_file) {
    NK_LOG_TRC("%s", __func__);

    NklLexer lexer{.data = nullptr, .proc = lexer_proc};
    NklParser parser{.data = nullptr, .proc = parser_proc};

    auto nkl = nkl_state_create(lexer, parser);
    defer {
        nkl_state_free(nkl);
    };

    auto c = nkl_createCompiler(nkl, {});
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
