#include "ntk/stream.h"

#include "ntk/profiler.h"
#include "stb/sprintf.h"

i32 nk_printf(NkStream out, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = nk_vprintf(out, fmt, ap);
    va_end(ap);

    return res;
}

typedef struct {
    NkStream out;
    char *buf;
} SprintfContext;

static char *sprintfCallback(char const *buf, void *user, i32 len) {
    char *ret;
    NK_PROF_FUNC() {
        SprintfContext *ctx = user;
        i32 res = nk_stream_write(ctx->out, (char *)buf, len);
        ret = res < 0 ? NULL : ctx->buf;
    }
    return ret;
}

i32 nk_vprintf(NkStream out, char const *fmt, va_list ap) {
    i32 ret;
    NK_PROF_FUNC() {
        char buf[STB_SPRINTF_MIN];
        SprintfContext ctx = {out, buf};
        ret = stbsp_vsprintfcb(sprintfCallback, &ctx, ctx.buf, fmt, ap);
    }
    return ret;
}
