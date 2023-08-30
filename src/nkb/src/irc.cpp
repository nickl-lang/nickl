#include "nkb/irc.h"

#include <cstdio>
#include <new>

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

    auto frame = nk_arena_grab(&c->file_arena);
    defer {
        nk_arena_popFrame(&c->file_arena, frame);
    };

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
    NK_LOG_INF("IR: %.*s", (int)ir_str.size, ir_str.data);
#endif // ENABLE_LOGGING

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

    // TODO Support different output formats
    (void)output_kind;

    if (!compileProgram(c, in_file)) {
        return 1;
    }

    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(sb, "gcc -x c -O2 -o %.*s -", (int)out_file.size, out_file.data);
    auto compile_cmd = nksb_concat(sb);

    auto pipe = popen(compile_cmd.data, "w");
    fprintf(pipe, R"(
        #include <stdio.h>
        int main(int argc, char** argv) {
            puts("Hello, World!");
            return 0;
        }
    )");
    pclose(pipe);

    return 0;
}

int nkir_run(NkIrCompiler c, nkstr in_file) {
    NK_LOG_TRC("%s", __func__);

    if (!compileProgram(c, in_file)) {
        return 1;
    }

    puts("Hello, World!");

    return 0;
}
