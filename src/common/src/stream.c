#include "nk/common/stream.h"

#include "stb/sprintf.h"

int nk_stream_printf(nk_stream out, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nk_stream_vprintf(out, fmt, ap);
    va_end(ap);

    return res;
}

struct SprintfCallbackContext {
    nk_stream out;
    char *buf;
};

static char *sprintfCallback(const char *buf, void *user, int len) {
    struct SprintfCallbackContext *context = user;
    int res = nk_stream_write(context->out, (char *)buf, len);
    return res < 0 ? NULL : context->buf;
}

int nk_stream_vprintf(nk_stream out, char const *fmt, va_list ap) {
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {out, buf};
    return stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
}

extern inline int nk_stream_read(nk_stream in, char *buf, size_t size);
extern inline int nk_stream_write(nk_stream out, char const *buf, size_t size);
