#include "irc.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <new>

#include <pthread.h>

#include "lexer.hpp"
#include "ntk/allocator.h"
#include "ntk/file.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/sys/path.h"
#include "ntk/sys/term.h"
#include "ntk/utils.h"
#include "parser.hpp"
#include "types.hpp"

namespace {

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(nkirc);

NK_PRINTF_LIKE(2, 3) void printError(NkIrCompiler c, char const *fmt, ...) {
    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    va_list ap;
    va_start(ap, fmt);
    nksb_vprintf(&sb, fmt, ap);
    va_end(ap);

    bool const to_color =
        c->opts.color_policy == NkIrcColor_Always || (c->opts.color_policy == NkIrcColor_Auto && nk_isatty(2));

    fprintf(
        stderr,
        "%serror:%s " nks_Fmt "\n",
        to_color ? NK_TERM_COLOR_RED : "",
        to_color ? NK_TERM_COLOR_NONE : "",
        nks_Arg(sb));
}

} // namespace

NkIrCompiler nkirc_create(NkIrcOptions opts) {
    NkIrCompiler c = new (nk_alloc_t<NkIrCompiler_T>(nk_default_allocator)) NkIrCompiler_T{
        .opts = opts,
    };
    c->parser.decls = decltype(NkIrParserState::decls)::create(nk_arena_getAllocator(&c->parse_arena));
    return c;
}

void nkirc_free(NkIrCompiler c) {
    c->fpmap.deinit();

    nk_arena_free(&c->parse_arena);
    nk_arena_free(&c->file_arena);
    nk_arena_free(&c->tmp_arena);

    nk_free_t(nk_default_allocator, c);
}

int nkir_compile(NkIrCompiler c, nks in_file, nks out_file, NkbOutputKind output_kind) {
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(nk_arena_getAllocator(&c->file_arena));

    auto base_path_str = (fs::current_path() / "_").string();
    nks base_file{base_path_str.c_str(), base_path_str.size()};

    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (output_kind == NkbOutput_Executable && c->entry_point.idx == NKIR_INVALID_IDX) {
        printError(c, "entry point is not defined");
        return false;
    }

    if (!nkir_write(&c->tmp_arena, c->ir, output_kind, out_file)) {
        return 1;
    }

    return 0;
}

int nkir_run(NkIrCompiler c, nks in_file) {
    NK_LOG_TRC("%s", __func__);

    c->ir = nkir_createProgram(nk_arena_getAllocator(&c->file_arena));

    auto base_path_str = (fs::current_path() / "_").string();
    nks base_file{base_path_str.c_str(), base_path_str.size()};
    if (!nkir_compileFile(c, base_file, in_file)) {
        return 1;
    }

    if (c->entry_point.idx == NKIR_INVALID_IDX) {
        printError(c, "entry point is not defined");
        return false;
    }

    auto run_ctx = nkir_createRunCtx(c->ir, &c->tmp_arena);
    defer {
        nkir_freeRunCtx(run_ctx);
    };

    // TODO Hardcoded extern symbols
    nkir_defineExternSym(run_ctx, cs2nkid("puts"), (void *)puts);
    nkir_defineExternSym(run_ctx, cs2nkid("printf"), (void *)printf);
    nkir_defineExternSym(run_ctx, cs2nkid("pthread_create"), (void *)pthread_create);
    nkir_defineExternSym(run_ctx, cs2nkid("pthread_join"), (void *)pthread_join);
    nkir_defineExternSym(run_ctx, cs2nkid("pthread_exit"), (void *)pthread_exit);
    nkir_defineExternSym(run_ctx, cs2nkid("sqrt"), (void *)sqrt);

    int argc = 1;
    char const *argv[] = {""};
    int64_t ret_code = -1;

    void *args[] = {&argc, &argv};
    void *rets[] = {&ret_code};
    nkir_invoke(run_ctx, c->entry_point, args, rets);

    return ret_code;
}

bool nkir_compileFile(NkIrCompiler c, nks base_file, nks in_file) {
    NK_LOG_TRC("%s", __func__);

    auto in_file_path = fs::path{std_str(base_file)}.parent_path() / fs::path{std_str(in_file)};

    if (!fs::exists(in_file_path)) {
        auto const in_file_path_str = in_file_path.string();
        printError(c, "file `%s` doesn't exist", in_file_path_str.c_str());
        return false;
    }

    in_file_path = fs::canonical(in_file_path);
    auto const in_file_path_str = in_file_path.string();

    auto const in_file_s = nks{in_file_path_str.c_str(), in_file_path_str.size()};

    auto read_res = nk_file_read(nk_arena_getAllocator(&c->file_arena), in_file_s);
    if (!read_res.ok) {
        printError(c, "failed to read file `%s`", in_file_path_str.c_str());
        return false;
    }

    auto const in_file_id = s2nkid(in_file_s);

    NkIrLexerState lexer{};
    {
        auto frame = nk_arena_grab(&c->tmp_arena);
        defer {
            nk_arena_popFrame(&c->tmp_arena, frame);
        };
        nkir_lex(&lexer, &c->file_arena, &c->tmp_arena, read_res.bytes);
        if (!lexer.ok) {
            printError(c, nks_Fmt, nks_Arg(lexer.error_msg));
            return false;
        }
    }

    {
        auto frame = nk_arena_grab(&c->tmp_arena);
        defer {
            nk_arena_popFrame(&c->tmp_arena, frame);
        };
        nkir_parse(c, in_file_id, {nkav_init(lexer.tokens)});
        if (!c->parser.ok) {
            printError(c, nks_Fmt, nks_Arg(c->parser.error_msg));
            return false;
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{};
    sb.alloc = nk_arena_getAllocator(&c->tmp_arena);
    nkir_inspectProgram(c->ir, &sb);
    NK_LOG_INF("IR:\n" nks_Fmt, nks_Arg(sb));
#endif // ENABLE_LOGGING

    return true;
}
