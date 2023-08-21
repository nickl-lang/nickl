#include "nkb/irc.h"

#include <cstdio>
#include <new>

#include "nk/common/allocator.hpp"
#include "nk/common/file.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/sys/term.h"

struct NkIrCompiler_T {
    NkIrcOptions opts;
};

namespace {

NK_LOG_USE_SCOPE(nkb);

NK_PRINTF_LIKE(2, 3) void printError(NkIrCompiler c, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto str = string_vformat(fmt, ap);
    va_end(ap);

    bool const to_color =
        c->opts.color_policy == NkIrcColor_Always || (c->opts.color_policy == NkIrcColor_Auto && nk_isatty());

    std::fprintf(
        stderr,
        "%serror:%s %.*s\n",
        to_color ? NK_TERM_COLOR_RED : "",
        to_color ? NK_TERM_COLOR_NONE : "",
        (int)str.size(),
        str.c_str());
}

} // namespace

NkIrCompiler nkirc_create(NkIrcOptions opts) {
    return new (nk_alloc_t<NkIrCompiler_T>(nk_default_allocator)) NkIrCompiler_T{
        .opts = opts,
    };
}

void nkirc_free(NkIrCompiler c) {
    nk_free_t(nk_default_allocator, c);
}

int nkir_compile(NkIrCompiler c, nkstr in_file, nkstr out_file, NkbOutputKind output_kind) {
    NK_LOG_TRC("%s", __func__);

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

    auto res = nk_readFile(nk_default_allocator, in_file);
    if (!res.ok) {
        printError(c, "failed to read file `%.*s`", (int)in_file.size, in_file.data);
        return 1;
    }
    defer {
        nk_free(nk_default_allocator, (void *)res.data.data, res.data.size);
    };
    NK_LOG_INF("%.*s", (int)res.data.size, res.data.data);

    puts("Hello, World!");

    return 0;
}
