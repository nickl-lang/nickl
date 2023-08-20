#include "nkb/nkb.h"

#include <cstdio>

#include "nk/common/logger.h"

NK_LOG_USE_SCOPE(nkb);

void nkb_compile(nkstr in_file, nkstr out_file, NkbOutputKind output_kind) {
    NK_LOG_TRC("%s", __func__);
    puts("Hello, World!");
}
