#ifndef HEADER_GUARD_NKIRC_DIAGNOSTICS
#define HEADER_GUARD_NKIRC_DIAGNOSTICS

#include <stdarg.h>

#include "ntk/common.h"
#include "ntk/string.h"

#ifndef __cplusplus
extern "C" {
#endif

typedef enum {
    NkIrcColor_Auto,
    NkIrcColor_Always,
    NkIrcColor_Never,
} NkIrcColorPolicy;

void nkirc_diag_init(NkIrcColorPolicy color_policy);

typedef struct {
    nks file;
    size_t lin;
    size_t col;
    size_t len;
} NkIrcSourceLocation;

NK_PRINTF_LIKE(1, 2) void nkirc_diag_printError(char const *fmt, ...);
NK_PRINTF_LIKE(2, 3) void nkirc_diag_printErrorFile(NkIrcSourceLocation loc, char const *fmt, ...);
NK_PRINTF_LIKE(3, 4) void nkirc_diag_printErrorQuote(nks src, NkIrcSourceLocation loc, char const *fmt, ...);

void nkirc_diag_vprintError(char const *fmt, va_list ap);
void nkirc_diag_vprintErrorFile(NkIrcSourceLocation loc, char const *fmt, va_list ap);
void nkirc_diag_vprintErrorQuote(nks src, NkIrcSourceLocation loc, char const *fmt, va_list ap);

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_nkirc_diag
