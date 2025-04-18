#include "ntk/string.h"

#include <ctype.h>

#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NkString nks_copy(NkAllocator alloc, NkString src) {
    char *mem = (char *)nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return (NkString){mem, src.size};
}

NkString nks_copyNt(NkAllocator alloc, NkString src) {
    char *mem = (char *)nk_alloc(alloc, src.size + 1);
    memcpy(mem, src.data, src.size);
    mem[src.size] = '\0';
    return (NkString){(char const *)mem, src.size};
}

NkString nks_trimLeft(NkString str) {
    while (str.size && nks_first(str) == ' ') {
        str.size -= 1;
        str.data += 1;
    }
    return str;
}

NkString nks_trimRight(NkString str) {
    while (str.size && nks_last(str) == ' ') {
        str.size -= 1;
    }
    return str;
}

NkString nks_trim(NkString str) {
    return nks_trimRight(nks_trimLeft(str));
}

NkString nks_chopByDelim(NkString *str, char delim) {
    usize i = 0;
    while (i < str->size && str->data[i] != delim) {
        i++;
    }

    NkString res = {str->data, i};

    if (i < str->size) {
        str->data += i + 1;
        str->size -= i + 1;
    } else {
        str->data += i;
        str->size -= i;
    }

    return res;
}

NkString nks_chopByDelimReverse(NkString *str, char delim) {
    NkString res = *str;

    while (str->size && nks_last(*str) != delim) {
        str->size--;
    }

    if (str->size) {
        str->size--;

        res.data += str->size + 1;
        res.size -= str->size + 1;
    } else {
        res.data += str->size;
        res.size -= str->size;
    }

    return res;
}

u64 nks_hash(NkString str) {
    return nk_hashArray((u8 *)&str.data[0], (u8 *)&str.data[str.size]);
}

bool nks_equal(NkString lhs, NkString rhs) {
    return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
}

bool nks_startsWith(NkString str, NkString pref) {
    if (pref.size > str.size) {
        return false;
    } else {
        return nks_equal((NkString){str.data, pref.size}, pref);
    }
}

#define WRITE(expr)        \
    do {                   \
        i32 _res = (expr); \
        if (_res < 0) {    \
            return -1;     \
        }                  \
        res += _res;       \
    } while (0)

i32 nks_escape(NkStream out, NkString str) {
    NK_PROF_FUNC_BEGIN();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        switch (str.data[i]) {
            case '\a':
                WRITE(nk_stream_writeCStr(out, "\\a"));
                break;
            case '\b':
                WRITE(nk_stream_writeCStr(out, "\\b"));
                break;
            case '\f':
                WRITE(nk_stream_writeCStr(out, "\\f"));
                break;
            case '\n':
                WRITE(nk_stream_writeCStr(out, "\\n"));
                break;
            case '\r':
                WRITE(nk_stream_writeCStr(out, "\\r"));
                break;
            case '\t':
                WRITE(nk_stream_writeCStr(out, "\\t"));
                break;
            case '\v':
                WRITE(nk_stream_writeCStr(out, "\\v"));
                break;
            case '\0':
                WRITE(nk_stream_writeCStr(out, "\\0"));
                break;
            case '\"':
                WRITE(nk_stream_writeCStr(out, "\\\""));
                break;
            case '\\':
                WRITE(nk_stream_writeCStr(out, "\\\\"));
                break;
            default:
                if (isprint(str.data[i])) {
                    WRITE(nk_stream_write(out, &str.data[i], 1));
                } else {
                    WRITE(nk_stream_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
                }
                break;
        }
    }
    NK_PROF_FUNC_END();
    return res;
}

i32 nks_unescape(NkStream out, NkString str) {
    NK_PROF_FUNC_BEGIN();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        if (str.data[i] == '\\' && i < str.size - 1) {
            switch (str.data[++i]) {
                case 'a':
                    WRITE(nk_stream_writeCStr(out, "\a"));
                    break;
                case 'b':
                    WRITE(nk_stream_writeCStr(out, "\b"));
                    break;
                case 'f':
                    WRITE(nk_stream_writeCStr(out, "\f"));
                    break;
                case 'n':
                    WRITE(nk_stream_writeCStr(out, "\n"));
                    break;
                case 'r':
                    WRITE(nk_stream_writeCStr(out, "\r"));
                    break;
                case 't':
                    WRITE(nk_stream_writeCStr(out, "\t"));
                    break;
                case 'v':
                    WRITE(nk_stream_writeCStr(out, "\v"));
                    break;
                case '0':
                    WRITE(nk_stream_write(out, "\0", 1));
                    break;
                default:
                    WRITE(nk_stream_write(out, &str.data[i], 1));
                    break;
            }
        } else {
            WRITE(nk_stream_write(out, &str.data[i], 1));
        }
    }
    NK_PROF_FUNC_END();
    return res;
}

i32 nks_sanitize(NkStream out, NkString str) {
    NK_PROF_FUNC_BEGIN();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        if (isprint(str.data[i])) {
            WRITE(nk_stream_write(out, &str.data[i], 1));
        } else {
            WRITE(nk_stream_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
        }
    }
    NK_PROF_FUNC_END();
    return res;
}

char const *nk_tprintf(NkArena *tmp_arena, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char const *str = nk_vtprintf(tmp_arena, fmt, ap);
    va_end(ap);

    return str;
}

char const *nk_vtprintf(NkArena *tmp_arena, char const *fmt, va_list ap) {
    return nk_vtsprintf(tmp_arena, fmt, ap).data;
}

NK_PRINTF_LIKE(2) NkString nk_tsprintf(NkArena *tmp_arena, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    NkString str = nk_vtsprintf(tmp_arena, fmt, ap);
    va_end(ap);

    return str;
}

NkString nk_vtsprintf(NkArena *tmp_arena, char const *fmt, va_list ap) {
    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(nk_arena_getAllocator(tmp_arena))};
    nksb_vprintf(&sb, fmt, ap);
    nksb_appendNull(&sb);
    return (NkString){sb.data, sb.size - 1};
}
