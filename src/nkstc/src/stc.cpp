#include "stc.h"

#include <filesystem>

#include "lexer.h"
#include "nkl/common/diagnostics.h"
#include "nkl/core/compiler.h"
#include "ntk/allocator.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkstc);

} // namespace

int nkst_compile(NkString in_file) {
    NK_LOG_TRC("%s", __func__);

    NkArena tmp_arena{};
    defer {
        nk_arena_free(&tmp_arena);
    };

    NkArena file_arena{};
    defer {
        nk_arena_free(&file_arena);
    };

    auto in_file_path = fs::current_path() / fs::path{nk_s2stdStr(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return 1;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = NkString{in_file_path_str.c_str(), in_file_path_str.size()};
    auto const in_file_id = nk_s2atom(in_file_s);

    auto const read_res = nk_file_read(nk_arena_getAllocator(&file_arena), in_file_s);
    if (!read_res.ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return 1;
    }

    auto const text = read_res.bytes;

    NkStLexerState lexer{};
    {
        auto frame = nk_arena_grab(&tmp_arena);
        defer {
            nk_arena_popFrame(&tmp_arena, frame);
        };
        if (!nkst_lex(&lexer, &file_arena, &tmp_arena, in_file_id, text)) {
            nkl_diag_printErrorQuote(
                text,
                {
                    in_file_s,
                    nk_slice_last(lexer.tokens).lin,
                    nk_slice_last(lexer.tokens).col,
                    nk_slice_last(lexer.tokens).len,
                },
                NKS_FMT,
                NKS_ARG(lexer.error_msg));
            return 1;
        }
    }

    NkStParserState parser{};
    {
        auto frame = nk_arena_grab(&tmp_arena);
        defer {
            nk_arena_popFrame(&tmp_arena, frame);
        };
        if (!nkst_parse(&parser, &file_arena, &tmp_arena, text, {NK_SLICE_INIT(lexer.tokens)})) {
            nkl_diag_printErrorQuote(
                text,
                {
                    in_file_s,
                    parser.error_token.lin,
                    parser.error_token.col,
                    parser.error_token.len,
                },
                NKS_FMT,
                NKS_ARG(parser.error_msg));
            return 1;
        }
    }

    auto c = nkl_createCompiler({});
    defer {
        nkl_freeCompiler(c);
    };

    auto m = nkl_createModule(c);
    nkl_compile(
        m,
        {
            .text = text,
            .tokens{NK_SLICE_INIT(lexer.tokens)},
            .nodes{NK_SLICE_INIT(parser.nodes)},
        });

    nkl_writeModule(m, nk_cs2s("a.out"));

    return 0;
}
