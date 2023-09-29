#include "nk/common/stream.h"

#include <string.h>

#include "stb/sprintf.h"

int nk_stream_printf(nk_ostream s, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nk_stream_vprintf(s, fmt, ap);
    va_end(ap);

    return res;
}

struct SprintfCallbackContext {
    nk_ostream stream;
    char *buf;
};

static char *sprintfCallback(const char *buf, void *user, int len) {
    struct SprintfCallbackContext *context = user;
    nk_stream_buf dst = context->stream.proc(context->stream.data, len);
    memcpy(dst.data, buf, minu(dst.size, len));
    return dst.size < (size_t)len ? NULL : context->buf;
}

int nk_stream_vprintf(nk_ostream s, char const *fmt, va_list ap) {
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {s, buf};
    return stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
}
