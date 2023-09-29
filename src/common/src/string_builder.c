#include "nk/common/string_builder.h"

#include <ctype.h>
#include <string.h>

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
    char* buf;
};

static char *sprintfCallback(const char *buf, void *user, int len) {
    struct SprintfCallbackContext *context = user;
    NkStringBuilder *sb = context->sb;

    if (sb->size + len > sb->capacity) {
        size_t new_capacity = ceilToPowerOf2(maxu(sb->size + len, NKSB_INIT_CAP));

        NkAllocator const alloc = sb->alloc.proc ? sb->alloc : nk_default_allocator;
        NkAllocatorSpaceLeftQueryResult query_res = {0};
        nk_alloc_querySpaceLeft(alloc, &query_res);

        if (query_res.kind == NkAllocatorSpaceLeft_Limited) {
            new_capacity = minu(new_capacity, sb->capacity + query_res.bytes_left);
        }

        nkar_maybe_grow(sb, new_capacity);
    }

    size_t const alloc_size = minu(len, sb->capacity - sb->size);
    memcpy(sb->data + sb->size, buf, alloc_size);
    return alloc_size < (size_t)len ? NULL : context->buf;
}

int nksb_vprintf(NkStringBuilder *sb, char const *fmt, va_list ap) {
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {sb, buf};
    int const printf_res = stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
    sb->size += printf_res;
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

static nk_stream_buf nksb_ostreamProc(void *s, size_t size) {
    NkStringBuilder *sb = (NkStringBuilder *)s;
    nksb_reserve(sb, sb->capacity + size);
    sb->size += size;
    return (nk_stream_buf){sb->data + sb->size - size, size};
}

nk_ostream nksb_getStream(NkStringBuilder *sb) {
    return (nk_ostream){sb, nksb_ostreamProc};
}
