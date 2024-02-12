#ifndef NTK_STRING_BUILDER_H_
#define NTK_STRING_BUILDER_H_

#include <stdarg.h>

#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/stream.h"
#include "ntk/string.h"

typedef NkDynArray(char) NkStringBuilder;

#define nksb_reserve nkda_reserve
#define nksb_append nkda_append
#define nksb_tryAppend nkda_tryAppend
#define nksb_appendMany nkda_appendMany
#define nksb_tryAppendMany nkda_tryAppendMany
#define nksb_free nkda_free
#define nksb_clear nkda_clear

#define nksb_appendNull(sb) nksb_append((sb), '\0')
#define nksb_tryAppendNull(sb) nksb_tryAppend((sb), '\0')

#define _nksb_appendStr(sb, str)                     \
    do {                                             \
        NkString _str = (str);                       \
        nksb_appendMany((sb), _str.data, _str.size); \
    } while (0)

#define _nksb_tryAppendStr(sb, str)                     \
    do {                                                \
        NkString _str = (str);                          \
        nksb_tryAppendMany((sb), _str.data, _str.size); \
    } while (0)

#define _nksb_appendCStr(sb, str) _nksb_appendStr(sb, nk_cs2s(str));
#define _nksb_tryAppendCStr(sb, str) _nksb_tryAppendStr(sb, nk_cs2s(str));

#define NKSB_INIT(allocator) .data = NULL, .size = 0, .capacity = 0, .alloc = (allocator)

#define NKSB_FIXED_BUFFER(NAME, SIZE)                  \
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

NK_PRINTF_LIKE(2, 3) i32 nksb_printf(NkStringBuilder *sb, char const *fmt, ...);
i32 nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap);

NkStream nksb_getStream(NkStringBuilder *sb);

bool nksb_readFromStream(NkStringBuilder *sb, NkStream in);
bool nksb_readFromStreamEx(NkStringBuilder *sb, NkStream in, usize buf_size);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

inline void nksb_appendStr(NkStringBuilder *sb, NkString str) {
    _nksb_appendStr(sb, str);
}
inline void nksb_tryAppendStr(NkStringBuilder *sb, NkString str) {
    _nksb_tryAppendStr(sb, str);
}

inline void nksb_appendCStr(NkStringBuilder *sb, char const *str) {
    _nksb_appendCStr(sb, str);
}
inline void nksb_tryAppendCStr(NkStringBuilder *sb, char const *str) {
    _nksb_tryAppendCStr(sb, str);
}

#else //__cplusplus

#define nksb_appendStr _nksb_appendStr
#define nksb_tryAppendStr _nksb_tryAppendStr

#define nksb_appendCStr _nksb_appendCStr
#define nksb_tryAppendCStr _nksb_tryAppendCStr

#endif //__cplusplus

#endif // NTK_STRING_BUILDER_H_
