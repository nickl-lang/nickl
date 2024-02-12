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
    NK_PROF_FUNC_BEGIN();
    struct SprintfCallbackContext *context = user;
    i32 res = nk_stream_write(context->out, (char *)buf, len);
    NK_PROF_FUNC_END();
    return res < 0 ? NULL : context->buf;
}

i32 nk_stream_vprintf(NkStream out, char const *fmt, va_list ap) {
    NK_PROF_FUNC_BEGIN();
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {out, buf};
    i32 ret = stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
    NK_PROF_FUNC_END();
    return ret;
}

extern inline i32 nk_stream_read(NkStream in, char *buf, usize size);
extern inline i32 nk_stream_write(NkStream out, char const *buf, usize size);
extern inline i32 nk_stream_writeCStr(NkStream out, char const *str);
