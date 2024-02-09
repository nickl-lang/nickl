#ifndef HEADER_GUARD_NTK_STRING_BUILDER
#define HEADER_GUARD_NTK_STRING_BUILDER

#include <stdarg.h>
#include <stddef.h>

#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#define nksb_reserve nkar_reserve
#define nksb_append nkar_append
#define nksb_try_append nkar_try_append
#define nksb_append_many nkar_append_many
#define nksb_try_append_many nkar_try_append_many
#define nksb_free nkar_free
#define nksb_clear nkar_clear

#define nksb_append_null(sb) nksb_append((sb), '\0')
#define nksb_try_append_null(sb) nksb_try_append((sb), '\0')

#define _nksb_append_str(sb, str)                     \
    do {                                              \
        nks _str = (str);                             \
        nksb_append_many((sb), _str.data, _str.size); \
    } while (0)

#define _nksb_try_append_str(sb, str)                     \
    do {                                                  \
        nks _str = (str);                                 \
        nksb_try_append_many((sb), _str.data, _str.size); \
    } while (0)

#define _nksb_append_cstr(sb, str) _nksb_append_str(sb, nk_cs2s(str));
#define _nksb_try_append_cstr(sb, str) _nksb_try_append_str(sb, nk_cs2s(str));

#define nksb_init(allocator) .data = NULL, .size = 0, .capacity = 0, .alloc = (allocator)

#define nksb_fixed_buffer(NAME, SIZE)                  \
    u8 _buf[SIZE];                                     \
    NkArena _arena = {_buf, 0, sizeof(_buf)};          \
    NkStringBuilder NAME = {                           \
        (char *)nk_arena_alloc(&_arena, sizeof(_buf)), \
        0,                                             \
        sizeof(_buf),                                  \
        nk_arena_getAllocator(&_arena),                \
    }

#ifdef __cplusplus
extern "C" {
#endif

nkar_typedef(char, NkStringBuilder);

NK_PRINTF_LIKE(2, 3) i32 nksb_printf(NkStringBuilder *sb, char const *fmt, ...);
i32 nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap);

nk_stream nksb_getStream(NkStringBuilder *sb);

bool nksb_readFromStream(NkStringBuilder *sb, nk_stream in);
bool nksb_readFromStreamEx(NkStringBuilder *sb, nk_stream in, usize buf_size);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

inline void nksb_append_str(NkStringBuilder *sb, nks str) {
    _nksb_append_str(sb, str);
}
inline void nksb_try_append_str(NkStringBuilder *sb, nks str) {
    _nksb_try_append_str(sb, str);
}

inline void nksb_append_cstr(NkStringBuilder *sb, char const *str) {
    _nksb_append_cstr(sb, str);
}
inline void nksb_try_append_cstr(NkStringBuilder *sb, char const *str) {
    _nksb_try_append_cstr(sb, str);
}

#else //__cplusplus

#define nksb_append_str _nksb_append_str
#define nksb_try_append_str _nksb_try_append_str

#define nksb_append_cstr _nksb_append_cstr
#define nksb_try_append_cstr _nksb_try_append_cstr

#endif //__cplusplus

#endif // HEADER_GUARD_NTK_STRING_BUILDER
