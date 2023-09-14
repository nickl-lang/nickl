#include "nkb/irc.h"

#include <cmath>
#include <cstdio>
#include <new>

#include <pthread.h>

#include "lexer.hpp"
#include "nk/common/allocator.h"
#include "nk/common/allocator.hpp"
#include "nk/common/file.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/sys/term.h"
#include "nkb/ir.h"
#include "parser.hpp"

struct NkIrCompiler_T {
    NkIrcOptions opts;
    NkIrProg ir{};
    NkIrProc entry_point{};
    NkArena tmp_arena{};
    NkArena file_arena{};
};

namespace {

NK_LOG_USE_SCOPE(nkirc);

NK_PRINTF_LIKE(2, 3) void printError(NkIrCompiler c, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto str = string_vformat(fmt, ap);
    va_end(ap);

    bool const to_color =
        c->opts.color_policy == NkIrcColor_Always || (c->opts.color_policy == NkIrcColor_Auto && nk_isatty());

    fprintf(
        stderr,
        "%serror:%s %.*s\n",
        to_color ? NK_TERM_COLOR_RED : "",
        to_color ? NK_TERM_COLOR_NONE : "",
        (int)str.size(),
        str.c_str());
}

bool compileProgram(NkIrCompiler c, nkstr in_file) {
    NK_LOG_TRC("%s", __func__);

    auto read_res = nk_readFile(nk_arena_getAllocator(&c->file_arena), in_file);
    if (!read_res.ok) {
        printError(c, "failed to read file `%.*s`", (int)in_file.size, in_file.data);
        return false;
    }

    NkIrLexerState lexer{};
    {
        auto frame = nk_arena_grab(&c->tmp_arena);
        defer {
            nk_arena_popFrame(&c->tmp_arena, frame);
        };
        nkir_lex(&lexer, &c->file_arena, &c->tmp_arena, read_res.bytes);
        if (!lexer.ok) {
            printError(c, "%.*s", (int)lexer.error_msg.size, lexer.error_msg.data);
            return false;
        }
    }

    NkIrParserState parser{};
    {
        auto frame = nk_arena_grab(&c->tmp_arena);
        defer {
            nk_arena_popFrame(&c->tmp_arena, frame);
        };
        nkir_parse(&parser, &c->file_arena, &c->tmp_arena, lexer.tokens);
        if (!parser.ok) {
            printError(c, "%.*s", (int)parser.error_msg.size, parser.error_msg.data);
            return false;
        }
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder_T sb{};
    nksb_init_alloc(&sb, nk_arena_getAllocator(&c->tmp_arena));
    nkir_inspectProgram(parser.ir, &sb);
    auto ir_str = nksb_concat(&sb);
    NK_LOG_INF("IR:\n%.*s", (int)ir_str.size, ir_str.data);
#endif // ENABLE_LOGGING

    c->ir = parser.ir;
    c->entry_point = parser.entry_point;

    return true;
}

} // namespace

NkIrCompiler nkirc_create(NkIrcOptions opts) {
    return new (nk_alloc_t<NkIrCompiler_T>(nk_default_allocator)) NkIrCompiler_T{
        .opts = opts,
    };
}

void nkirc_free(NkIrCompiler c) {
    nk_arena_free(&c->file_arena);
    nk_arena_free(&c->tmp_arena);

    nk_free_t(nk_default_allocator, c);
}

int nkir_compile(NkIrCompiler c, nkstr in_file, nkstr out_file, NkbOutputKind output_kind) {
    NK_LOG_TRC("%s", __func__);

    if (!compileProgram(c, in_file)) {
        return 1;
    }

    if (!nkir_write(c->ir, output_kind, out_file)) {
        return 1;
    }

    return 0;
}

int nkir_run(NkIrCompiler c, nkstr in_file) {
    NK_LOG_TRC("%s", __func__);

    if (!compileProgram(c, in_file)) {
        return 1;
    }

    auto run_ctx = nkir_createRunCtx(c->ir, &c->tmp_arena);
    defer {
        nkir_freeRunCtx(run_ctx);
    };

    // TODO Hardcoded extern symbols
    nkir_defineExternSym(run_ctx, nk_mkstr("puts"), (void *)puts);
    nkir_defineExternSym(run_ctx, nk_mkstr("printf"), (void *)printf);
    nkir_defineExternSym(run_ctx, nk_mkstr("pthread_create"), (void *)pthread_create);
    nkir_defineExternSym(run_ctx, nk_mkstr("pthread_join"), (void *)pthread_join);
    nkir_defineExternSym(run_ctx, nk_mkstr("pthread_exit"), (void *)pthread_exit);
    nkir_defineExternSym(run_ctx, nk_mkstr("sqrt"), (void *)sqrt);

    int argc = 1;
    char const *argv[] = {""};
    int ret_code = -1;

    void *args[] = {&argc, &argv};
    void *rets[] = {&ret_code};
    nkir_invoke(run_ctx, c->entry_point, args, rets);

    return ret_code;
}
