#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <stdarg.h>

#include "nk/common/allocator.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkStringBuilder_T *NkStringBuilder;

NkStringBuilder nksb_create();
NkStringBuilder nksb_create_alloc(NkAllocator *alloc);

void nksb_free(NkStringBuilder sb);

int nksb_printf(NkStringBuilder sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder sb, char const *fmt, va_list ap);

nkstr nksb_concat(NkStringBuilder sb);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
