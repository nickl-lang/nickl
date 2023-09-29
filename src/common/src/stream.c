#include "nk/common/stream.h"

#include "stb/sprintf.h"

int nk_stream_printf(nk_ostream ostream, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nk_stream_vprintf(ostream, fmt, ap);
    va_end(ap);

    return res;
}

struct SprintfCallbackContext {
    nk_ostream ostream;
    char *buf;
};

static char *sprintfCallback(const char *buf, void *user, int len) {
    struct SprintfCallbackContext *context = user;
    size_t bytes_written = context->ostream.proc(context->ostream.data, buf, len);
    return bytes_written != (size_t)len ? NULL : context->buf;
}

int nk_stream_vprintf(nk_ostream ostream, char const *fmt, va_list ap) {
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {ostream, buf};
    return stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
}
