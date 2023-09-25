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

nkar_typedef(char, NkStringBuilder_T);

// TODO Rename NkStringBuilder_T -> NkStringBuilder
typedef NkStringBuilder_T *NkStringBuilder;

NK_PRINTF_LIKE(2, 3) int nksb_printf(NkStringBuilder_T *sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder_T *sb, char const *fmt, va_list ap);

void nksb_str_escape(NkStringBuilder_T *sb, nkstr str);
void nksb_str_unescape(NkStringBuilder_T *sb, nkstr str);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
