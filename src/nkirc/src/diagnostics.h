#ifndef HEADER_GUARD_NKIRC_DIAGNOSTICS
#define HEADER_GUARD_NKIRC_DIAGNOSTICS

#include <stdarg.h>

#include "ntk/common.h"
#include "ntk/string.h"

#ifndef __cplusplus
extern "C" {
#endif

enum NkIrcColorPolicy {
    NkIrcColor_Auto,
    NkIrcColor_Always,
    NkIrcColor_Never,
};

void nkirc_diag_init(NkIrcColorPolicy color_policy);

NK_PRINTF_LIKE(1, 2) void nkirc_diag_printError(char const *fmt, ...);
NK_PRINTF_LIKE(4, 5) void nkirc_diag_printErrorFile(nks file, size_t lin, size_t col, char const *fmt, ...);
NK_PRINTF_LIKE(6, 7)
void nkirc_diag_printErrorQuote(nks src, nks file, size_t lin, size_t col, size_t len, char const *fmt, ...);

void nkirc_diag_vprintError(char const *fmt, va_list ap);
void nkirc_diag_vprintErrorFile(nks file, size_t lin, size_t col, char const *fmt, va_list ap);
void nkirc_diag_vprintErrorQuote(nks src, nks file, size_t lin, size_t col, size_t len, char const *fmt, va_list ap);

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_nkirc_diag
