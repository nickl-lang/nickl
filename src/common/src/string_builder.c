#include "nk/common/string_builder.h"

#include <ctype.h>
#include <string.h>

#include "nk/common/stream.h"
#include "nk/common/utils.h"
#include "stb/sprintf.h"

int nksb_printf(NkStringBuilder *sb, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nksb_vprintf(sb, fmt, ap);
    va_end(ap);

    return res;
}

#define NKSB_INIT_CAP 1000

struct SprintfCallbackContext {
    NkStringBuilder *sb;
    char *buf;
};

static char *sprintfCallback(const char *buf, void *user, int len) {
    struct SprintfCallbackContext *context = user;
    NkStringBuilder *sb = context->sb;
    size_t prev_size = sb->size;
    nksb_try_append_many(sb, buf, len);
    return (sb->size - prev_size) < (size_t)len ? NULL : context->buf;
}

int nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap) {
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {sb, buf};
    int const printf_res = stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
    return printf_res;
}

void nksb_str_escape(NkStringBuilder *sb, nkstr str) {
    for (size_t i = 0; i < str.size; i++) {
        switch (str.data[i]) {
        case '\a':
            nksb_try_append_str(sb, "\\a");
            break;
        case '\b':
            nksb_try_append_str(sb, "\\b");
            break;
        case '\f':
            nksb_try_append_str(sb, "\\f");
            break;
        case '\n':
            nksb_try_append_str(sb, "\\n");
            break;
        case '\r':
            nksb_try_append_str(sb, "\\r");
            break;
        case '\t':
            nksb_try_append_str(sb, "\\t");
            break;
        case '\v':
            nksb_try_append_str(sb, "\\v");
            break;
        case '\0':
            nksb_try_append_str(sb, "\\0");
            break;
        case '\"':
            nksb_try_append_str(sb, "\\\"");
            break;
        case '\\':
            nksb_try_append_str(sb, "\\\\");
            break;
        default:
            if (isprint(str.data[i])) {
                nksb_try_append(sb, str.data[i]);
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
                nksb_try_append(sb, '\a');
                break;
            case 'b':
                nksb_try_append(sb, '\b');
                break;
            case 'f':
                nksb_try_append(sb, '\f');
                break;
            case 'n':
                nksb_try_append(sb, '\n');
                break;
            case 'r':
                nksb_try_append(sb, '\r');
                break;
            case 't':
                nksb_try_append(sb, '\t');
                break;
            case 'v':
                nksb_try_append(sb, '\v');
                break;
            case '0':
                nksb_try_append(sb, '\0');
                break;
            default:
                nksb_try_append(sb, str.data[i]);
                break;
            }
        } else {
            nksb_try_append(sb, str.data[i]);
        }
    }
}

static int nksb_streamProc(void *stream_data, char *buf, size_t size, nk_stream_mode mode) {
    if (mode == nk_stream_mode_write) {
        NkStringBuilder *sb = (NkStringBuilder *)stream_data;
        size_t prev_size = sb->size;
        nksb_try_append_many(sb, buf, size);
        return sb->size - prev_size;
    } else {
        return -1;
    }
}

nk_stream nksb_getStream(NkStringBuilder *sb) {
    return (nk_stream){sb, nksb_streamProc};
}

#define NKSB_STREAM_BUF_SIZE 1024

void nksb_readFromStream(NkStringBuilder *sb, nk_stream in) {
    for (;;) {
        nksb_reserve(sb, sb->capacity + NKSB_STREAM_BUF_SIZE);
        int res = nk_stream_read(in, nkav_end(sb), NKSB_STREAM_BUF_SIZE);
        if (res > 0) {
            sb->size += res;
        } else {
            break;
        }
    }
}
