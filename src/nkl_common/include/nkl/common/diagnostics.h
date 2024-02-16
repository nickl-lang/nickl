#ifndef NKL_COMMON_DIAGNOSTICS_H_
#define NKL_COMMON_DIAGNOSTICS_H_

#include <stdarg.h>

#include "ntk/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklColorPolicy_Auto,
    NklColorPolicy_Always,
    NklColorPolicy_Never,
} NklColorPolicy;

void nkl_diag_init(NklColorPolicy color_policy);

typedef struct {
    NkString file;
    u32 lin;
    u32 col;
    u32 len;
} NklSourceLocation;

NK_PRINTF_LIKE(1, 2) void nkl_diag_printError(char const *fmt, ...);
NK_PRINTF_LIKE(2, 3) void nkl_diag_printErrorFile(NklSourceLocation loc, char const *fmt, ...);
NK_PRINTF_LIKE(3, 4) void nkl_diag_printErrorQuote(NkString src, NklSourceLocation loc, char const *fmt, ...);

void nkl_diag_vprintError(char const *fmt, va_list ap);
void nkl_diag_vprintErrorFile(NklSourceLocation loc, char const *fmt, va_list ap);
void nkl_diag_vprintErrorQuote(NkString src, NklSourceLocation loc, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_DIAGNOSTICS_H_
