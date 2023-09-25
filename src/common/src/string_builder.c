#include "nk/common/string_builder.h"

#include <ctype.h>

int nksb_printf(NkStringBuilder *sb, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nksb_vprintf(sb, fmt, ap);
    va_end(ap);

    return res;
}

int nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap) {
    va_list ap_copy;

    va_copy(ap_copy, ap);
    int const printf_res = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (printf_res < 0) {
        return printf_res;
    }

    size_t const required_size = printf_res + 1;

    nksb_reserve(sb, required_size);

    va_copy(ap_copy, ap);
    vsnprintf(sb->data + sb->size, required_size, fmt, ap_copy);
    va_end(ap_copy);

    sb->size += required_size - 1;

    return printf_res;
}

void nksb_str_escape(NkStringBuilder *sb, nkstr str) {
    for (size_t i = 0; i < str.size; i++) {
        switch (str.data[i]) {
        case '\a':
            nksb_printf(sb, "\\a");
            break;
        case '\b':
            nksb_printf(sb, "\\b");
            break;
        case '\f':
            nksb_printf(sb, "\\f");
            break;
        case '\n':
            nksb_printf(sb, "\\n");
            break;
        case '\r':
            nksb_printf(sb, "\\r");
            break;
        case '\t':
            nksb_printf(sb, "\\t");
            break;
        case '\v':
            nksb_printf(sb, "\\v");
            break;
        case '\0':
            nksb_printf(sb, "\\0");
            break;
        case '\"':
            nksb_printf(sb, "\\\"");
            break;
        case '\\':
            nksb_printf(sb, "\\\\");
            break;
        default:
            if (isprint(str.data[i])) {
                nksb_printf(sb, "%c", str.data[i]);
            } else {
                nksb_printf(sb, "\\x%" PRIx8, str.data[i] & 0xff);
            }
            break;
        }
    }
}

void nksb_str_unescape(NkStringBuilder *sb, nkstr str) {
    for (size_t i = 0; i < str.size; i++) {
        if (str.data[i] == '\\' && i < str.size - 1) {
            switch (str.data[++i]) {
            case 'a':
                nksb_printf(sb, "%c", '\a');
                break;
            case 'b':
                nksb_printf(sb, "%c", '\b');
                break;
            case 'f':
                nksb_printf(sb, "%c", '\f');
                break;
            case 'n':
                nksb_printf(sb, "%c", '\n');
                break;
            case 'r':
                nksb_printf(sb, "%c", '\r');
                break;
            case 't':
                nksb_printf(sb, "%c", '\t');
                break;
            case 'v':
                nksb_printf(sb, "%c", '\v');
                break;
            case '0':
                nksb_printf(sb, "%c", '\0');
                break;
            default:
                nksb_printf(sb, "%c", str.data[i]);
                break;
            }
        } else {
            nksb_printf(sb, "%c", str.data[i]);
        }
    }
}
