#include "ntk/stream.h"

#include "ntk/profiler.h"
#include "stb/sprintf.h"

i32 nk_printf(nk_stream out, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nk_vprintf(out, fmt, ap);
    va_end(ap);

    return res;
}

struct SprintfCallbackContext {
    nk_stream out;
    char *buf;
};

static char *sprintfCallback(char const *buf, void *user, i32 len) {
    ProfBeginFunc();
    struct SprintfCallbackContext *context = user;
    i32 res = nk_stream_write(context->out, (char *)buf, len);
    ProfEndBlock();
    return res < 0 ? NULL : context->buf;
}

i32 nk_vprintf(nk_stream out, char const *fmt, va_list ap) {
    ProfBeginFunc();
    char buf[STB_SPRINTF_MIN];
    struct SprintfCallbackContext context = {out, buf};
    i32 ret = stbsp_vsprintfcb(sprintfCallback, &context, context.buf, fmt, ap);
    ProfEndBlock();
    return ret;
}

extern inline i32 nk_stream_read(nk_stream in, char *buf, usize size);
extern inline i32 nk_stream_write(nk_stream out, char const *buf, usize size);
extern inline i32 nk_stream_write_str(nk_stream out, char const *str);
