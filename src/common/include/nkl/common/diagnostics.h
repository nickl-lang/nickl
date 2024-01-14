#ifndef HEADER_GUARD_NKL_COMMON_DIAGNOSTICS
#define HEADER_GUARD_NKL_COMMON_DIAGNOSTICS

#include <stdarg.h>

#include "ntk/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklColor_Auto,
    NklColor_Always,
    NklColor_Never,
} NklColorPolicy;

void nkl_diag_init(NklColorPolicy color_policy);

typedef struct {
    nks file;
    size_t lin;
    size_t col;
    size_t len;
} NklSourceLocation;

NK_PRINTF_LIKE(1, 2) void nkl_diag_printError(char const *fmt, ...);
NK_PRINTF_LIKE(2, 3) void nkl_diag_printErrorFile(NklSourceLocation loc, char const *fmt, ...);
NK_PRINTF_LIKE(3, 4) void nkl_diag_printErrorQuote(nks src, NklSourceLocation loc, char const *fmt, ...);

void nkl_diag_vprintError(char const *fmt, va_list ap);
void nkl_diag_vprintErrorFile(NklSourceLocation loc, char const *fmt, va_list ap);
void nkl_diag_vprintErrorQuote(nks src, NklSourceLocation loc, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_DIAGNOSTICS
