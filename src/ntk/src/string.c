#include "ntk/string.h"

#include <ctype.h>

#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
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

bool nks_equalCStr(NkString lhs, char const *rhs) {
    while (*rhs) {
        if (lhs.size == 0 || *rhs != *lhs.data) {
            return false;
        }
        lhs.data++;
        lhs.size--;
        rhs++;
    }
    return lhs.size == 0;
}

NkString nks_left(NkString str, usize n) {
    usize const size = nk_minu(str.size, n);
    return (NkString){str.data, size};
}

NkString nks_right(NkString str, usize n) {
    usize const size = nk_minu(str.size, n);
    return (NkString){str.data + str.size - size, size};
}

bool nks_startsWith(NkString str, NkString prefix) {
    if (prefix.size > str.size) {
        return false;
    } else {
        return nks_equal(nks_left(str, prefix.size), prefix);
    }
}

bool nks_endsWith(NkString str, NkString suffix) {
    if (suffix.size > str.size) {
        return false;
    } else {
        return nks_equal(nks_right(str, suffix.size), suffix);
    }
}

#define WRITE(expr)        \
    do {                   \
        i32 _res = (expr); \
        if (_res < 0) {    \
            return -1;     \
        }                  \
        ret += _res;       \
    } while (0)

i32 nks_escape(NkStream out, NkString str) {
    i32 ret = 0;
    NK_PROF_FUNC() {
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
                        WRITE(nk_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
                    }
                    break;
            }
        }
    }
    return ret;
}

i32 nks_unescape(NkStream out, NkString str) {
    i32 ret = 0;
    NK_PROF_FUNC() {
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
    }
    return ret;
}

i32 nks_sanitize(NkStream out, NkString str) {
    i32 ret = 0;
    NK_PROF_FUNC() {
        for (usize i = 0; i < str.size; i++) {
            if (isprint(str.data[i])) {
                WRITE(nk_stream_write(out, &str.data[i], 1));
            } else {
                WRITE(nk_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
            }
        }
    }
    return ret;
}

NkStringArray nks_shell_lex(NkArena *arena, NkString str) {
    NkDynArray(NkString) tokens = {.alloc = nk_arena_getAllocator(arena)};
    NkStringBuilder token = {.alloc = nk_arena_getAllocator(arena)};

    char quote = 0;
    for (usize i = 0; i < str.size; i++) {
        char const c = str.data[i];

        if (c == '\\' && i + 1 < str.size) {
            nksb_append(&token, str.data[++i]);
        }

        else if (quote) {
            if (c == quote) {
                quote = 0;
            } else {
                nksb_append(&token, c);
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        }

        else if (isspace(c)) {
            if (token.data) {
                nksb_appendNull(&token);
                nkda_append(&tokens, ((NkString){NKS_INIT(token)}));

                token = (NkStringBuilder){.alloc = nk_arena_getAllocator(arena)};
            }
        }

        else {
            nksb_append(&token, c);
        }
    }

    if (token.data) {
        nksb_appendNull(&token);
        nkda_append(&tokens, ((NkString){NKS_INIT(token)}));
    }

    return (NkStringArray){NK_SLICE_INIT(tokens)};
}

char const *nk_tprintf(NkArena *arena, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char const *str = nk_vtprintf(arena, fmt, ap);
    va_end(ap);

    return str;
}

char const *nk_vtprintf(NkArena *arena, char const *fmt, va_list ap) {
    return nk_vtsprintf(arena, fmt, ap).data;
}

NK_PRINTF_LIKE(2) NkString nk_tsprintf(NkArena *arena, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    NkString str = nk_vtsprintf(arena, fmt, ap);
    va_end(ap);

    return str;
}

NkString nk_vtsprintf(NkArena *arena, char const *fmt, va_list ap) {
    NkStringBuilder sb = (NkStringBuilder){.alloc = nk_arena_getAllocator(arena)};
    nksb_vprintf(&sb, fmt, ap);
    nksb_appendNull(&sb);
    nk_arena_pop(arena, sb.capacity - sb.size);
    return (NkString){sb.data, sb.size - 1};
}
