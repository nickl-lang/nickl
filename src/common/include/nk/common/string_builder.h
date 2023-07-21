#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <stdarg.h>

#include "nk/common/allocator.h"
#include "nk/common/common.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkStringBuilder_T *NkStringBuilder;

NkStringBuilder nksb_create();
NkStringBuilder nksb_create_alloc(NkAllocator alloc);

void nksb_free(NkStringBuilder sb);

NK_PRINTF_LIKE(2, 3) int nksb_printf(NkStringBuilder sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder sb, char const *fmt, va_list ap);

nkstr nksb_concat(NkStringBuilder sb);

void nksb_str_escape(NkStringBuilder sb, nkstr str);
void nksb_str_unescape(NkStringBuilder sb, nkstr str);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
