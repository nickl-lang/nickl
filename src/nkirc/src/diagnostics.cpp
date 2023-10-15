#include "diagnostics.h"

#include "ntk/file.h"
#include "ntk/string.h"
#include "ntk/sys/file.h"
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
    nk_stream out = nk_file_getStream(nk_stderr());
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_RED);
    }
    nk_stream_printf(out, "error:");
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_NONE);
    }
    nk_stream_printf(out, " ");
    nk_stream_vprintf(out, fmt, ap);
    nk_stream_printf(out, "\n");
}

void nkirc_diag_vprintErrorFile(nks file, size_t lin, size_t col, char const *fmt, va_list ap) {
    bool const to_color = toColor();
    nk_stream out = nk_file_getStream(nk_stderr());
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_WHITE);
    }
    nk_stream_printf(out, nks_Fmt, nks_Arg(file));
    if (lin) {
        nk_stream_printf(out, ":%zu", lin);
    }
    if (col) {
        nk_stream_printf(out, ":%zu", col);
    }
    nk_stream_printf(out, ":");
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_NONE);
    }
    nk_stream_printf(out, " ");

    nkirc_diag_vprintError(fmt, ap);
}

#define MAX_LINE_QUOTE 120

void nkirc_diag_vprintErrorQuote(nks src, nks file, size_t lin, size_t col, size_t len, char const *fmt, va_list ap) {
    nkirc_diag_vprintErrorFile(file, lin, col, fmt, ap);

    nk_stream out = nk_file_getStream(nk_stderr());

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
        int line_offset = nk_stream_printf(out, "%5zu |", lin);
        int pointer_offset = col;
        int actual_len = len;
        if (col && col <= line.size && len) {
            pointer_offset = nks_escape(out, {line.data, col - 1}) + 1;
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_RED);
            }
            actual_len = nks_escape(out, {line.data + col - 1, len});
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_NONE);
            }
            nks_escape(out, {line.data + col - 1 + len, line.size - col + 1 - len});
        } else {
            nks_escape(out, line);
        }
        nk_stream_printf(out, "\n");
        if (col) {
            nk_stream_printf(out, "%*c", line_offset, '|');
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_RED);
            }
            nk_stream_printf(out, "%*c", pointer_offset, '^');
            if (actual_len > 0) {
                for (size_t i = 0; i < (size_t)actual_len - 1; i++) {
                    nk_stream_printf(out, "~");
                }
            }
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_NONE);
            }
            nk_stream_printf(out, "\n");
        }
    }
}
