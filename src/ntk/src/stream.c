#include "ntk/stream.h"

#include "ntk/profiler.h"
#include "stb/sprintf.h"

i32 nk_stream_printf(NkStream out, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nk_stream_vprintf(out, fmt, ap);
    va_end(ap);

    return res;
}

struct SprintfCallbackContext {
    NkStream out;
    char *buf;
};

static char *sprintfCallback(char const *buf, void *user, i32 len) {
    char *ret;
    NK_PROF_FUNC() {
        struct SprintfCallbackContext *context = user;
        i32 res = nk_stream_write(context->out, (char *)buf, len);
        ret = res < 0 ? NULL : context->buf;
    }
    return ret;
}

i32 nk_stream_vprintf(NkStream out, char const *fmt, va_list ap) {
    i32 ret;
    NK_PROF_FUNC() {
        char buf[STB_SPRINTF_MIN];
        struct SprintfCallbackContext context = {out, buf};
        ret = stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
    }
    return ret;
}
