#ifndef NTK_STREAM_H_
#define NTK_STREAM_H_

#include <string.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkStreamMode_Read,
    NkStreamMode_Write,
} NkStreamMode;

typedef i32 (*NkStreamProc)(void *stream_data, char *buf, usize size, NkStreamMode mode);

typedef struct {
    void *data;
    NkStreamProc proc;
} NkStream;

NK_INLINE i32 nk_stream_read(NkStream in, char *buf, usize size) {
    return in.proc(in.data, buf, size, NkStreamMode_Read);
}

NK_INLINE i32 nk_stream_write(NkStream out, char const *buf, usize size) {
    return out.proc(out.data, (char *)buf, size, NkStreamMode_Write);
}

NK_INLINE i32 nk_stream_writeCStr(NkStream out, char const *str) {
    return nk_stream_write(out, str, strlen(str));
}

NK_EXPORT NK_PRINTF_LIKE(2) i32 nk_stream_printf(NkStream out, char const *fmt, ...);
NK_EXPORT i32 nk_stream_vprintf(NkStream out, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NTK_STREAM_H_
