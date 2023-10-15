#ifndef HEADER_GUARD_NKIRC_DIAGNOSTICS
#define HEADER_GUARD_NKIRC_DIAGNOSTICS

#include "ntk/common.h"

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

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_nkirc_diag
