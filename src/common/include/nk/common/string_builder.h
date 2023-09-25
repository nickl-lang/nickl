#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <stdarg.h>

#include "nk/common/array.h"
#include "nk/common/string.h"
#include "nk/sys/common.h"

#define nksb_reserve nkar_reserve
#define nksb_append nkar_append
#define nksb_append_many nkar_append_many
#define nksb_free nkar_free

#define nksb_append_null(sb) nksb_append((sb), '\0')

#ifdef __cplusplus
extern "C" {
#endif

nkar_typedef(char, NkStringBuilder);

NK_PRINTF_LIKE(2, 3) int nksb_printf(NkStringBuilder *sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap);

void nksb_str_escape(NkStringBuilder *sb, nkstr str);
void nksb_str_unescape(NkStringBuilder *sb, nkstr str);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
