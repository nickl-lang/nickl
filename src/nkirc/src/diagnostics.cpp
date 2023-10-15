#include "diagnostics.h"

#include <stdarg.h>
#include <stdio.h>

#include "ntk/sys/term.h"

static NkIrcColorPolicy s_color_policy;

void nkirc_diag_init(NkIrcColorPolicy color_policy) {
    s_color_policy = color_policy;
}

void nkirc_diag_printError(char const *fmt, ...) {
    bool const to_color = s_color_policy == NkIrcColor_Always || (s_color_policy == NkIrcColor_Auto && nk_isatty(2));

    fprintf(stderr, "%serror:%s ", to_color ? NK_TERM_COLOR_RED : "", to_color ? NK_TERM_COLOR_NONE : "");

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}
