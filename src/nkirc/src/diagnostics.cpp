#include "diagnostics.h"

#include <stdio.h>

#include "ntk/string.h"
#include "ntk/sys/term.h"

static NkIrcColorPolicy s_color_policy;

static bool toColor() {
    return s_color_policy == NkIrcColor_Always || (s_color_policy == NkIrcColor_Auto && nk_isatty(2));
}

void nkirc_diag_init(NkIrcColorPolicy color_policy) {
    s_color_policy = color_policy;
}

void nkirc_diag_printError(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkirc_diag_vprintError(fmt, ap);
    va_end(ap);
}

void nkirc_diag_printErrorFile(nks file, size_t lin, size_t col, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkirc_diag_vprintErrorFile(file, lin, col, fmt, ap);
    va_end(ap);
}

void nkirc_diag_printErrorQuote(nks src, nks file, size_t lin, size_t col, size_t len, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkirc_diag_vprintErrorQuote(src, file, lin, col, len, fmt, ap);
    va_end(ap);
}

void nkirc_diag_vprintError(char const *fmt, va_list ap) {
    bool const to_color = toColor();
    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_RED);
    }
    fprintf(stderr, "error:");
    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE);
    }
    fprintf(stderr, " ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void nkirc_diag_vprintErrorFile(nks file, size_t lin, size_t col, char const *fmt, va_list ap) {
    bool const to_color = toColor();
    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_WHITE);
    }
    fprintf(stderr, nks_Fmt, nks_Arg(file));
    if (lin) {
        fprintf(stderr, ":%zu", lin);
    }
    if (col) {
        fprintf(stderr, ":%zu", col);
    }
    fprintf(stderr, ":");
    if (to_color) {
        fprintf(stderr, NK_TERM_COLOR_NONE);
    }
    fprintf(stderr, " ");

    nkirc_diag_vprintError(fmt, ap);
}

#define MAX_LINE_QUOTE 120

void nkirc_diag_vprintErrorQuote(nks src, nks file, size_t lin, size_t col, size_t len, char const *fmt, va_list ap) {
    nkirc_diag_vprintErrorFile(file, lin, col, fmt, ap);

    // TODO Sanitize output

    bool const to_color = toColor();

    nks line{};
    for (size_t i = 0; i < lin; i++) {
        line = nks_chop_by_delim(&src, '\n');
    }
    if (line.size) {
        if (col && col <= line.size && col > MAX_LINE_QUOTE / 2) {
            line.data += col - MAX_LINE_QUOTE / 2;
            line.size -= col - MAX_LINE_QUOTE / 2;
            col = MAX_LINE_QUOTE / 2;
        }
        if (line.size > MAX_LINE_QUOTE) {
            line.size = MAX_LINE_QUOTE;
        }
        if (col && col > line.size) {
            col = line.size;
        }
        if (col && len && len > line.size - col) {
            len = line.size - col + 1;
        }
        int line_offset = fprintf(stderr, "%5zu |", lin);
        if (col && len && col <= line.size) {
            fprintf(stderr, nks_Fmt, (int)col - 1, line.data);
            if (to_color) {
                fprintf(stderr, NK_TERM_COLOR_RED);
            }
            fprintf(stderr, nks_Fmt, (int)len, line.data + col - 1);
            if (to_color) {
                fprintf(stderr, NK_TERM_COLOR_NONE);
            }
            fprintf(stderr, nks_Fmt, (int)(line.size - col + 1 - len), line.data + col - 1 + len);
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, nks_Fmt "\n", nks_Arg(line));
        }
        if (col && line_offset > 0) {
            fprintf(stderr, "%*c", line_offset, '|');
            if (to_color) {
                fprintf(stderr, NK_TERM_COLOR_RED);
            }
            fprintf(stderr, "%*c", (int)col, '^');
            if (len) {
                for (size_t i = 0; i < len - 1; i++) {
                    fprintf(stderr, "~");
                }
            }
            if (to_color) {
                fprintf(stderr, NK_TERM_COLOR_NONE);
            }
            fprintf(stderr, "\n");
        }
    }
}
