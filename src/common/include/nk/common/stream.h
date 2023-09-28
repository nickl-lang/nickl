#ifndef HEADER_GUARD_NK_COMMON_STREAM
#define HEADER_GUARD_NK_COMMON_STREAM

#include <stdarg.h>

#include "nk/common/string.h"
#include "nk/sys/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *data;
    size_t size;
} nk_stream_buf;

typedef size_t (*nk_istream_proc)(void *s, nk_stream_buf buf);
typedef nk_stream_buf (*nk_ostream_proc)(void *s, size_t size);

typedef struct {
    void *data;
    nk_ostream_proc proc;
} nk_ostream;

typedef struct {
    void *data;
    nk_istream_proc proc;
} nk_istream;

int nk_stream_vprintf(nk_ostream s, char const *fmt, va_list ap);

NK_PRINTF_LIKE(2, 3) int nk_stream_printf(nk_ostream s, char const *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STREAM
