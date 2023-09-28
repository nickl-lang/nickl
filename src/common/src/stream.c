#include "nk/common/stream.h"

#include "stb/sprintf.h"

int nk_stream_printf(nk_ostream s, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = nk_stream_vprintf(s, fmt, ap);
    va_end(ap);

    return res;
}

int nk_stream_vprintf(nk_ostream s, char const *fmt, va_list ap) {
    va_list ap_copy;

    va_copy(ap_copy, ap);
    int const printf_res = stbsp_vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (printf_res < 0) {
        return printf_res;
    }

    nk_stream_buf buf = s.proc(s.data, printf_res);

    va_copy(ap_copy, ap);
    stbsp_vsnprintf(buf.data, buf.size, fmt, ap_copy);
    va_end(ap_copy);

    return printf_res;
}
