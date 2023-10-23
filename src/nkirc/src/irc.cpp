#include "irc.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <new>

#include "irc_impl.hpp"
#include "lexer.h"
#include "nkb/ir.h"
#include "nkl/common/diagnostics.h"
#include "ntk/allocator.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/path.h"
#include "ntk/sys/term.h"
#include "ntk/utils.h"
#include "parser.h"
#include "types.h"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkirc);

} // namespace

NkIrCompiler nkirc_create(NkArena *tmp_arena, NkIrcConfig conf) {
    NkIrCompiler c = new (nk_alloc_t<NkIrCompiler_T>(nk_default_allocator)) NkIrCompiler_T{
        .tmp_arena = tmp_arena,
        .conf = conf,
    };
    c->parser.decls = decltype(NkIrParserState::decls)::create(nk_arena_getAllocator(&c->parse_arena));
    return c;
}

void nkirc_free(NkIrCompiler c) {
    c->fpmap.deinit();

    nk_arena_free(&c->parse_arena);
    nk_arena_free(&c->file_arena);

    nk_free_t(nk_default_allocator, c);
}

int nkir_compile(NkIrCompiler c, nks in_file, NkIrCompilerConfig conf) {
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(&c->file_arena);

    auto base_path_str = (fs::current_path() / "_").string();
    nks base_file{base_path_str.c_str(), base_path_str.size()};

    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (conf.output_kind == NkbOutput_Executable && c->entry_point.idx == NKIR_INVALID_IDX) {
        nkl_diag_printError("entry point is not defined");
        return false;
    }

    if (!nkir_write(c->ir, c->tmp_arena, conf)) {
        nks err_str = nkir_getErrorString(c->ir);
        nkl_diag_printError("failed to run write output: " nks_Fmt, nks_Arg(err_str));
        return 1;
    }

    return 0;
}

int nkir_run(NkIrCompiler c, nks in_file) {
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(&c->file_arena);

    auto base_path_str = (fs::current_path() / "_").string();
    nks base_file{base_path_str.c_str(), base_path_str.size()};
    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (c->entry_point.idx == NKIR_INVALID_IDX) {
        nkl_diag_printError("entry point is not defined");
        return false;
    }

    auto run_ctx = nkir_createRunCtx(c->ir, c->tmp_arena);
    defer {
        nkir_freeRunCtx(run_ctx);
    };

    int argc = 1;
    char const *argv[] = {""};
    int64_t ret_code = -1;

    void *args[] = {&argc, &argv};
    void *rets[] = {&ret_code};
    if (!nkir_invoke(run_ctx, c->entry_point, args, rets)) {
        nks err_str = nkir_getRunErrorString(run_ctx);
        nkl_diag_printError("failed to run the program: " nks_Fmt, nks_Arg(err_str));
        return 1;
    }

    return ret_code;
}

bool nkir_compileFile(NkIrCompiler c, nks base_file, nks in_file) {
    NK_LOG_TRC("%s", __func__);

    auto in_file_path = fs::path{std_str(base_file)}.parent_path() / fs::path{std_str(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        nkl_diag_printError("file `%s` doesn't exist", in_file_path_str.c_str());
        return false;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = nks{in_file_path_str.c_str(), in_file_path_str.size()};

    auto read_res = nk_file_read(nk_arena_getAllocator(&c->file_arena), in_file_s);
    if (!read_res.ok) {
        nkl_diag_printError("failed to read file `%s`", in_file_path_str.c_str());
        return false;
    }

    auto const in_file_id = s2nkid(in_file_s);

    NkIrLexerState lexer{};
    {
        auto frame = nk_arena_grab(c->tmp_arena);
        defer {
            nk_arena_popFrame(c->tmp_arena, frame);
        };
        nkir_lex(&lexer, &c->file_arena, c->tmp_arena, read_res.bytes);
        if (!lexer.ok) {
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
            return false;
        }
    }

    {
        auto frame = nk_arena_grab(c->tmp_arena);
        defer {
            nk_arena_popFrame(c->tmp_arena, frame);
        };
        nkir_parse(c, in_file_id, {nkav_init(lexer.tokens)});
        if (!c->parser.ok) {
            nkl_diag_printErrorQuote(
                read_res.bytes,
                {
                    in_file_s,
                    c->parser.error_token.lin,
                    c->parser.error_token.col,
                    c->parser.error_token.text.size,
                },
                nks_Fmt,
                nks_Arg(c->parser.error_msg));
            return false;
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = nk_arena_getAllocator(c->tmp_arena);
    nkir_inspectProgram(c->ir, nksb_getStream(&sb));
    NK_LOG_INF("IR:\n" nks_Fmt, nks_Arg(sb));
#endif // ENABLE_LOGGING

    return true;
}
