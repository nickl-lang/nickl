#ifndef NTK_FILE_H_
#define NTK_FILE_H_

#include "ntk/allocator.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT bool nk_file_read(NkAllocator alloc, NkString filepath, NkString *out);

NK_EXPORT NkStream nk_file_getStream(NkHandle h_file);

extern char const *nk_null_file;

NK_EXPORT i32 nk_read(NkHandle fd, char *buf, usize n);
NK_EXPORT i32 nk_write(NkHandle fd, char const *buf, usize n);

typedef enum {
    NkOpenFlags_Read = 1,
    NkOpenFlags_Write = 2,
    NkOpenFlags_Create = 4,
    NkOpenFlags_Truncate = 8,
} NkOpenFlags;

NK_EXPORT NkHandle nk_open(char const *file, i32 flags);

NK_EXPORT i32 nk_close(NkHandle fd);

NK_EXPORT NkHandle nk_stdin(void);
NK_EXPORT NkHandle nk_stdout(void);
NK_EXPORT NkHandle nk_stderr(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_FILE_H_
