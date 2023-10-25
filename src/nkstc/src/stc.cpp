#include "stc.h"

#include <filesystem>

#include "lexer.h"
#include "nkl/common/diagnostics.h"
#include "ntk/allocator.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/utils.h"
#include "parser.h"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkstc);

} // namespace

int nkst_compile(nks in_file) {
    NK_LOG_TRC("%s", __func__);

    NkArena tmp_arena{};
    defer {
        nk_arena_free(&tmp_arena);
    };

    NkArena file_arena{};
    defer {
        nk_arena_free(&file_arena);
    };

    auto in_file_path = fs::current_path() / fs::path{std_str(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return 1;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = nks{in_file_path_str.c_str(), in_file_path_str.size()};
    auto const in_file_id = s2nkid(in_file_s);

    auto read_res = nk_file_read(nk_arena_getAllocator(&file_arena), in_file_s);
    if (!read_res.ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return 1;
    }

    NkStLexerState lexer{};
    {
        auto frame = nk_arena_grab(&tmp_arena);
        defer {
            nk_arena_popFrame(&tmp_arena, frame);
        };
        if (!nkst_lex(&lexer, &file_arena, &tmp_arena, in_file_id, read_res.bytes)) {
            nkl_diag_printErrorQuote(
                read_res.bytes,
                {
                    in_file_s,
                    nkav_last(lexer.tokens).lin,
                    nkav_last(lexer.tokens).col,
                    nkav_last(lexer.tokens).text.size,
                },
                nks_Fmt,
                nks_Arg(lexer.error_msg));
            return 1;
        }
    }

    NkStParserState parser{};
    {
        auto frame = nk_arena_grab(&tmp_arena);
        defer {
            nk_arena_popFrame(&tmp_arena, frame);
        };
        if (!nkst_parse(&parser, &file_arena, &tmp_arena, {nkav_init(lexer.tokens)})) {
            nkl_diag_printErrorQuote(
                read_res.bytes,
                {
                    in_file_s,
                    parser.error_token.lin,
                    parser.error_token.col,
                    parser.error_token.text.size,
                },
                nks_Fmt,
                nks_Arg(parser.error_msg));
            return 1;
        }
    }

    return 0;
}
