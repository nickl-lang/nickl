#include "nkb/nkb.h"

#include <cstdio>

#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"

NK_LOG_USE_SCOPE(nkb);

void nkb_compile(nkstr in_file, nkstr out_file, NkbOutputKind output_kind) {
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
}

void nkb_run(nkstr in_file) {
    NK_LOG_TRC("%s", __func__);

    puts("Hello, World!");
}
