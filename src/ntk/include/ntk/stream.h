#ifndef HEADER_GUARD_NTK_STREAM
#define HEADER_GUARD_NTK_STREAM

#include <stdarg.h>
#include <string.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    nk_stream_mode_read,
    nk_stream_mode_write,
} nk_stream_mode;

typedef int (*nk_stream_proc)(void *stream_data, char *buf, size_t size, nk_stream_mode mode);

typedef struct {
    void *data;
    nk_stream_proc proc;
} nk_stream;

NK_INLINE int nk_stream_read(nk_stream in, char *buf, size_t size) {
    return in.proc(in.data, buf, size, nk_stream_mode_read);
}

NK_INLINE int nk_stream_write(nk_stream out, char const *buf, size_t size) {
    return out.proc(out.data, (char *)buf, size, nk_stream_mode_write);
}

NK_PRINTF_LIKE(2, 3) int nk_printf(nk_stream out, char const *fmt, ...);
int nk_vprintf(nk_stream out, char const *fmt, va_list ap);

NK_INLINE int nk_stream_write_str(nk_stream out, char const *str) {
    return nk_stream_write(out, str, strlen(str));
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_STREAM
