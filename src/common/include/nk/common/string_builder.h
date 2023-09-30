#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include <stdarg.h>
#include <stddef.h>

#include "nk/common/allocator.h"
#include "nk/common/array.h"
#include "nk/common/stream.h"
#include "nk/common/string.h"
#include "nk/sys/common.h"

#define nksb_reserve nkar_reserve
#define nksb_append nkar_append
#define nksb_try_append nkar_try_append
#define nksb_append_many nkar_append_many
#define nksb_try_append_many nkar_try_append_many
#define nksb_free nkar_free

#define nksb_append_null(sb) nksb_append((sb), '\0')
#define nksb_try_append_null(sb) nksb_try_append((sb), '\0')

#define _nksb_append_str(sb, str)           \
    do {                                    \
        char const *_str = (str);           \
        size_t _len = strlen(_str);         \
        nksb_append_many((sb), _str, _len); \
    } while (0)

#define _nksb_try_append_str(sb, str)           \
    do {                                        \
        char const *_str = (str);               \
        size_t _len = strlen(_str);             \
        nksb_try_append_many((sb), _str, _len); \
    } while (0)

#define nksb_init(allocator) .data = NULL, .size = 0, .capacity = 0, .alloc = (allocator)

#define nksb_fixed_buffer(NAME, SIZE)                      \
    uint8_t _buf[SIZE];                                    \
    NkArena _log_arena = {_buf, 0, sizeof(_buf)};          \
    NkStringBuilder NAME = {                               \
        (char *)nk_arena_alloc(&_log_arena, sizeof(_buf)), \
        0,                                                 \
        sizeof(_buf),                                      \
        nk_arena_getAllocator(&_log_arena),                \
    }

#ifdef __cplusplus
extern "C" {
#endif

nkar_typedef(char, NkStringBuilder);

NK_PRINTF_LIKE(2, 3) int nksb_printf(NkStringBuilder *sb, char const *fmt, ...);
int nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap);

void nksb_str_escape(NkStringBuilder *sb, nkstr str);
void nksb_str_unescape(NkStringBuilder *sb, nkstr str);

nk_stream nksb_getStream(NkStringBuilder *sb);

void nksb_readFromStream(NkStringBuilder *sb, nk_stream in);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

inline void nksb_append_str(NkStringBuilder *sb, char const *str) {
    _nksb_append_str(sb, str);
}
inline void nksb_try_append_str(NkStringBuilder *sb, char const *str) {
    _nksb_try_append_str(sb, str);
}

#else //__cplusplus

#define nksb_append_str _nksb_append_str
#define nksb_try_append_str _nksb_try_append_str

#endif //__cplusplus

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
