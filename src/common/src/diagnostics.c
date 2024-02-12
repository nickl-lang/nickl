#include "nkl/common/diagnostics.h"

#include "ntk/file.h"
#include "ntk/os/file.h"
#include "ntk/os/term.h"
#include "ntk/string.h"

static NklColorPolicy s_color_policy;

static bool toColor() {
    return s_color_policy == NklColor_Always || (s_color_policy == NklColor_Auto && nk_isatty(2));
}

void nkl_diag_init(NklColorPolicy color_policy) {
    s_color_policy = color_policy;
}

void nkl_diag_printError(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkl_diag_vprintError(fmt, ap);
    va_end(ap);
}

void nkl_diag_printErrorFile(NklSourceLocation loc, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkl_diag_vprintErrorFile(loc, fmt, ap);
    va_end(ap);
}

void nkl_diag_printErrorQuote(NkString src, NklSourceLocation loc, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    nkl_diag_vprintErrorQuote(src, loc, fmt, ap);
    va_end(ap);
}

void nkl_diag_vprintError(char const *fmt, va_list ap) {
    bool const to_color = toColor();
    NkStream out = nk_file_getStream(nk_stderr());
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

void nkl_diag_vprintErrorFile(NklSourceLocation loc, char const *fmt, va_list ap) {
    bool const to_color = toColor();
    NkStream out = nk_file_getStream(nk_stderr());
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_WHITE);
    }
    nk_stream_printf(out, NKS_FMT, NKS_ARG(loc.file));
    if (loc.lin) {
        nk_stream_printf(out, ":%zu", loc.lin);
    }
    if (loc.col) {
        nk_stream_printf(out, ":%zu", loc.col);
    }
    nk_stream_printf(out, ":");
    if (to_color) {
        nk_stream_printf(out, NK_TERM_COLOR_NONE);
    }
    nk_stream_printf(out, " ");

    nkl_diag_vprintError(fmt, ap);
}

#define MAX_LINE_QUOTE 120

void nkl_diag_vprintErrorQuote(NkString src, NklSourceLocation loc, char const *fmt, va_list ap) {
    nkl_diag_vprintErrorFile(loc, fmt, ap);

    NkStream out = nk_file_getStream(nk_stderr());

    bool const to_color = toColor();

    NkString line = {0};
    for (usize i = 0; i < loc.lin; i++) {
        line = nks_chopByDelim(&src, '\n');
    }
    if (line.size) {
        if (loc.col && loc.col <= line.size && loc.col > MAX_LINE_QUOTE / 2) {
            line.data += loc.col - MAX_LINE_QUOTE / 2;
            line.size -= loc.col - MAX_LINE_QUOTE / 2;
            loc.col = MAX_LINE_QUOTE / 2;
        }
        if (line.size > MAX_LINE_QUOTE) {
            line.size = MAX_LINE_QUOTE;
        }
        if (loc.col && loc.col > line.size) {
            loc.col = line.size;
        }
        if (loc.col && loc.len && loc.len > line.size - loc.col) {
            loc.len = line.size - loc.col + 1;
        }
        int line_offset = nk_stream_printf(out, "%5zu | ", loc.lin);
        int pointer_offset = loc.col;
        int actual_len = loc.len;
        if (loc.col && loc.col <= line.size && loc.len) {
            pointer_offset = nks_sanitize(out, (NkString){line.data, loc.col - 1}) + 1;
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_RED);
            }
            actual_len = nks_sanitize(out, (NkString){line.data + loc.col - 1, loc.len});
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_NONE);
            }
            nks_sanitize(out, (NkString){line.data + loc.col - 1 + loc.len, line.size - loc.col + 1 - loc.len});
        } else {
            nks_sanitize(out, line);
        }
        nk_stream_printf(out, "\n");
        if (loc.col) {
            nk_stream_printf(out, "%*s", line_offset, "| ");
            if (to_color) {
                nk_stream_printf(out, NK_TERM_COLOR_RED);
            }
            nk_stream_printf(out, "%*c", pointer_offset, '^');
            if (actual_len > 0) {
                for (usize i = 0; i < (usize)actual_len - 1; i++) {
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