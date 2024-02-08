#include "ntk/string_builder.h"

#include <ctype.h>
#include <string.h>

#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/utils.h"
#include "stb/sprintf.h"

int nksb_printf(NkStringBuilder *sb, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nksb_vprintf(sb, fmt, ap);
    va_end(ap);

    return res;
}

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
    ProfBeginFunc();

    for (;;) {
        nksb_reserve(sb, sb->capacity + NKSB_STREAM_BUF_SIZE);
        int res = nk_stream_read(in, nkav_end(sb), NKSB_STREAM_BUF_SIZE);
        if (res > 0) {
            sb->size += res;
        } else {
            break;
        }
    }

    ProfEndBlock();
}
