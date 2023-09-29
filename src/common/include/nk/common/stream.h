#ifndef HEADER_GUARD_NK_COMMON_STREAM
#define HEADER_GUARD_NK_COMMON_STREAM

#include <stdarg.h>

#include "nk/common/string.h"
#include "nk/sys/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*nk_istream_proc)(void *istream_data, char *buf, size_t size);
typedef size_t (*nk_ostream_proc)(void *ostream_data, char const *buf, size_t size);

typedef struct {
    void *data;
    nk_ostream_proc proc;
} nk_ostream;

typedef struct {
    void *data;
    nk_istream_proc proc;
} nk_istream;

int nk_stream_vprintf(nk_ostream ostream, char const *fmt, va_list ap);
NK_PRINTF_LIKE(2, 3) int nk_stream_printf(nk_ostream ostream, char const *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_STREAM
