#include "stc.h"

#include <filesystem>

#include "lexer.h"
#include "nkl/common/diagnostics.h"
#include "nkl/core/compiler.h"
#include "nkl/core/nickl.h"
#include "ntk/arena.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

namespace fs = std::filesystem;

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

    NklErrorState error_state{};
    nkl_errorStateInitAndEquip(&error_state, &file_arena);

    auto in_file_path = fs::current_path() / fs::path{nk_s2stdStr(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return 1;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = NkString{in_file_path_str.c_str(), in_file_path_str.size()};

    auto const read_res = nk_file_read(nk_arena_getAllocator(&file_arena), in_file_s);
    if (!read_res.ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return 1;
    }

    auto const text = read_res.bytes;

    auto c = nkl_createCompiler(&nkl, {});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    if (!nkl_compileSrc(m, text)) {
        auto error = error_state.errors;
        while (error) {
            nkl_diag_printErrorQuote(
                text,
                {
                    in_file_s,
                    error->token->lin,
                    error->token->col,
                    error->token->len,
                },
                NKS_FMT,
                NKS_ARG(error->msg));
            error = error->next;
        }
        return 1;
    }

    nkl_writeModule(m, nk_cs2s("a.out"));

    return 0;
}
