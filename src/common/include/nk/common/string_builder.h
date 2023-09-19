#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <stdarg.h>

#include "nk/common/allocator.h"
#include "nk/common/common.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkStringBuilder_T {
    char *data;
    size_t size;
    size_t capacity;
    NkAllocator alloc;
} * NkStringBuilder;

NkStringBuilder_T *nksb_create();
NkStringBuilder_T *nksb_create_alloc(NkAllocator alloc);

void nksb_init(NkStringBuilder_T *sb);
void nksb_init_alloc(NkStringBuilder_T *sb, NkAllocator alloc);

void nksb_deinit(NkStringBuilder_T *sb);
void nksb_free(NkStringBuilder_T *sb);

NK_PRINTF_LIKE(2, 3) int nksb_printf(NkStringBuilder_T *sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder_T *sb, char const *fmt, va_list ap);

nkstr nksb_concat(NkStringBuilder_T *sb);

void nksb_str_escape(NkStringBuilder_T *sb, nkstr str);
void nksb_str_unescape(NkStringBuilder_T *sb, nkstr str);

#define NK_DEFINE_STATIC_SB(NAME, SIZE)                                                                      \
    uint8_t _buf[SIZE];                                                                                      \
    NkArena log_arena{_buf, 0, sizeof(_buf)};                                                                \
    NkStringBuilder_T NAME {                                                                                 \
        (char *)nk_arena_alloc(&log_arena, sizeof(_buf)), 0, sizeof(_buf), nk_arena_getAllocator(&log_arena) \
    }

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
