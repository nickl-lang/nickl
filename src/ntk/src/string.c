#include "ntk/string.h"

#include <ctype.h>

#define WRITE(expr)        \
    do {                   \
        int _res = (expr); \
        if (_res < 0) {    \
            return -1;     \
        }                  \
        res += _res;       \
    } while (0)

int nks_escape(nk_stream out, nks str) {
    int res = 0;
    for (size_t i = 0; i < str.size; i++) {
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
                WRITE(nk_stream_printf(out, "\\x%" PRIx8, str.data[i] & 0xff));
            }
            break;
        }
    }
    return res;
}

int nks_unescape(nk_stream out, nks str) {
    int res = 0;
    for (size_t i = 0; i < str.size; i++) {
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
    return res;
}

extern inline nks nk_strcpy(NkAllocator alloc, nks src);
extern inline nks nk_cs2s(char const *str);
extern inline nks nks_trim_left(nks str);
extern inline nks nks_trim_right(nks str);
extern inline nks nks_trim(nks str);
extern inline nks nks_chop_by_delim(nks *str, char delim);
extern inline nks nks_chop_by_delim_reverse(nks *str, char delim);
