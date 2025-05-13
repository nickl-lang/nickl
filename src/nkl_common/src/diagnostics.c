#include "nkl/common/diagnostics.h"

#include "ntk/file.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/term.h"

static NklColorPolicy s_color_policy;

static bool toColor(void) {
    return s_color_policy == NklColorPolicy_Always || (s_color_policy == NklColorPolicy_Auto && nk_isatty(2));
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
    char buf[512];
    NkFileStreamBuf stream_buf = {.file = nk_stderr(), .buf = buf, .size = sizeof(buf)};
    NkStream out = nk_file_getBufferedWriteStream(&stream_buf);

    bool const to_color = toColor();

    if (to_color) {
        nk_printf(out, NK_TERM_COLOR_RED);
    }
    nk_printf(out, "error:");
    if (to_color) {
        nk_printf(out, NK_TERM_COLOR_NONE);
    }
    nk_printf(out, " ");
    nk_vprintf(out, fmt, ap);
    nk_printf(out, "\n");

    nk_stream_flush(out);
}

void nkl_diag_vprintErrorFile(NklSourceLocation loc, char const *fmt, va_list ap) {
    char buf[512];
    NkFileStreamBuf stream_buf = {.file = nk_stderr(), .buf = buf, .size = sizeof(buf)};
    NkStream out = nk_file_getBufferedWriteStream(&stream_buf);

    if (loc.file.size) {
        bool const to_color = toColor();

        if (to_color) {
            nk_printf(out, NK_TERM_COLOR_WHITE);
        }
        nk_printf(out, NKS_FMT, NKS_ARG(loc.file));
        if (loc.lin) {
            nk_printf(out, ":%u", loc.lin);
        }
        if (loc.col) {
            nk_printf(out, ":%u", loc.col);
        }
        nk_printf(out, ":");
        if (to_color) {
            nk_printf(out, NK_TERM_COLOR_NONE);
        }
        nk_printf(out, " ");
    }

    nk_stream_flush(out);

    nkl_diag_vprintError(fmt, ap);
}

#define MAX_LINE_QUOTE 120

void nkl_diag_vprintErrorQuote(NkString src, NklSourceLocation loc, char const *fmt, va_list ap) {
    char buf[512];
    NkFileStreamBuf stream_buf = {.file = nk_stderr(), .buf = buf, .size = sizeof(buf)};
    NkStream out = nk_file_getBufferedWriteStream(&stream_buf);

    nkl_diag_vprintErrorFile(loc, fmt, ap);

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
        int line_offset = nk_printf(out, "%5u | ", loc.lin);
        int pointer_offset = loc.col;
        int actual_len = loc.len;
        if (loc.col && loc.col <= line.size && loc.len) {
            pointer_offset = nks_sanitize(out, (NkString){line.data, loc.col - 1}) + 1;
            if (to_color) {
                nk_printf(out, NK_TERM_COLOR_RED);
            }
            actual_len = nks_sanitize(out, (NkString){line.data + loc.col - 1, loc.len});
            if (to_color) {
                nk_printf(out, NK_TERM_COLOR_NONE);
            }
            nks_sanitize(out, (NkString){line.data + loc.col - 1 + loc.len, line.size - loc.col + 1 - loc.len});
        } else {
            nks_sanitize(out, line);
        }
        nk_printf(out, "\n");
        if (loc.col) {
            nk_printf(out, "%*s", line_offset, "| ");
            if (to_color) {
                nk_printf(out, NK_TERM_COLOR_RED);
            }
            nk_printf(out, "%*c", pointer_offset, '^');
            if (actual_len > 0) {
                for (usize i = 0; i < (usize)actual_len - 1; i++) {
                    nk_printf(out, "~");
                }
            }
            if (to_color) {
                nk_printf(out, NK_TERM_COLOR_NONE);
            }
            nk_printf(out, "\n");
        }
    }

    nk_stream_flush(out);
}
