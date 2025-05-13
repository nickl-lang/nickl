#ifndef NTK_FILE_H_
#define NTK_FILE_H_

#include "ntk/allocator.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT bool nk_file_read(NkAllocator alloc, NkString filepath, NkString *out);

NK_EXPORT NkStream nk_file_getStream(NkHandle file);

typedef struct {
    NkHandle file;
    char *buf;
    usize size;
    usize _used;
} NkFileStreamBuf;

NK_EXPORT NkStream nk_file_getBufferedWriteStream(NkFileStreamBuf *stream_buf);

extern char const *nk_null_file;

NK_EXPORT i32 nk_read(NkHandle file, char *buf, usize n);
NK_EXPORT i32 nk_write(NkHandle file, char const *buf, usize n);
NK_EXPORT i32 nk_flush(NkHandle file);

typedef enum {
    NkOpenFlags_Read = 1,
    NkOpenFlags_Write = 2,
    NkOpenFlags_Create = 4,
    NkOpenFlags_Truncate = 8,
} NkOpenFlags;

NK_EXPORT NkHandle nk_open(char const *path, i32 flags);

NK_EXPORT i32 nk_close(NkHandle file);

NK_EXPORT NkHandle nk_stdin(void);
NK_EXPORT NkHandle nk_stdout(void);
NK_EXPORT NkHandle nk_stderr(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_FILE_H_
