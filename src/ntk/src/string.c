#include "ntk/string.h"

#include <ctype.h>

#include "ntk/profiler.h"

nks nk_strcpy(NkAllocator alloc, nks src) {
    char *mem = (char *)nk_alloc(alloc, src.size);
    memcpy(mem, src.data, src.size);
    return LITERAL(nks){mem, src.size};
}

nks nk_strcpy_nt(NkAllocator alloc, nks src) {
    char *mem = (char *)nk_alloc(alloc, src.size + 1);
    memcpy(mem, src.data, src.size);
    mem[src.size] = '\0';
    return LITERAL(nks){(char const *)mem, src.size};
}

nks nks_trim_left(nks str) {
    while (str.size && nks_first(str) == ' ') {
        str.size -= 1;
        str.data += 1;
    }
    return str;
}

nks nks_trim_right(nks str) {
    while (str.size && nks_last(str) == ' ') {
        str.size -= 1;
    }
    return str;
}

nks nks_trim(nks str) {
    return nks_trim_right(nks_trim_left(str));
}

nks nks_chop_by_delim(nks *str, char delim) {
    usize i = 0;
    while (i < str->size && str->data[i] != delim) {
        i++;
    }

    nks res = {str->data, i};

    if (i < str->size) {
        str->data += i + 1;
        str->size -= i + 1;
    } else {
        str->data += i;
        str->size -= i;
    }

    return res;
}

nks nks_chop_by_delim_reverse(nks *str, char delim) {
    nks res = *str;

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

hash_t nks_hash(nks str) {
    return hash_array((u8 *)&str.data[0], (u8 *)&str.data[str.size]);
}

bool nks_equal(nks lhs, nks rhs) {
    return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
}

bool nks_starts_with(nks str, nks pref) {
    if (pref.size > str.size) {
        return false;
    } else {
        return nks_equal(LITERAL(nks){str.data, pref.size}, pref);
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

i32 nks_escape(nk_stream out, nks str) {
    ProfBeginFunc();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        switch (str.data[i]) {
        case '\a':
            WRITE(nk_stream_write_str(out, "\\a"));
            break;
        case '\b':
            WRITE(nk_stream_write_str(out, "\\b"));
            break;
        case '\f':
            WRITE(nk_stream_write_str(out, "\\f"));
            break;
        case '\n':
            WRITE(nk_stream_write_str(out, "\\n"));
            break;
        case '\r':
            WRITE(nk_stream_write_str(out, "\\r"));
            break;
        case '\t':
            WRITE(nk_stream_write_str(out, "\\t"));
            break;
        case '\v':
            WRITE(nk_stream_write_str(out, "\\v"));
            break;
        case '\0':
            WRITE(nk_stream_write_str(out, "\\0"));
            break;
        case '\"':
            WRITE(nk_stream_write_str(out, "\\\""));
            break;
        case '\\':
            WRITE(nk_stream_write_str(out, "\\\\"));
            break;
        default:
            if (isprint(str.data[i])) {
                WRITE(nk_stream_write(out, &str.data[i], 1));
            } else {
                WRITE(nk_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
            }
            break;
        }
    }
    ProfEndBlock();
    return res;
}

i32 nks_unescape(nk_stream out, nks str) {
    ProfBeginFunc();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        if (str.data[i] == '\\' && i < str.size - 1) {
            switch (str.data[++i]) {
            case 'a':
                WRITE(nk_stream_write_str(out, "\a"));
                break;
            case 'b':
                WRITE(nk_stream_write_str(out, "\b"));
                break;
            case 'f':
                WRITE(nk_stream_write_str(out, "\f"));
                break;
            case 'n':
                WRITE(nk_stream_write_str(out, "\n"));
                break;
            case 'r':
                WRITE(nk_stream_write_str(out, "\r"));
                break;
            case 't':
                WRITE(nk_stream_write_str(out, "\t"));
                break;
            case 'v':
                WRITE(nk_stream_write_str(out, "\v"));
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
    ProfEndBlock();
    return res;
}

i32 nks_sanitize(nk_stream out, nks str) {
    ProfBeginFunc();
    i32 res = 0;
    for (usize i = 0; i < str.size; i++) {
        if (isprint(str.data[i])) {
            WRITE(nk_stream_write(out, &str.data[i], 1));
        } else {
            WRITE(nk_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
        }
    }
    ProfEndBlock();
    return res;
}

extern inline nks nk_cs2s(char const *str);
