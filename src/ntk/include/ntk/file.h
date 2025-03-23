#ifndef NTK_FILE_H_
#define NTK_FILE_H_

#include "ntk/allocator.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO Refactor NkFileReadResult
typedef struct {
    NkString bytes;
    bool ok;
} NkFileReadResult;

NK_EXPORT NkFileReadResult nk_file_read(NkAllocator alloc, NkString file);

NK_EXPORT NkStream nk_file_getStream(NkOsHandle h_file);

extern char const *nk_null_file;

NK_EXPORT i32 nk_read(NkOsHandle fd, char *buf, usize n);
NK_EXPORT i32 nk_write(NkOsHandle fd, char const *buf, usize n);

typedef enum {
    NkOpenFlags_Read = 1,
    NkOpenFlags_Write = 2,
    NkOpenFlags_Create = 4,
    NkOpenFlags_Truncate = 8,
} NkOpenFlags;

NK_EXPORT NkOsHandle nk_open(char const *file, i32 flags);

NK_EXPORT i32 nk_close(NkOsHandle fd);

NK_EXPORT NkOsHandle nk_stdin(void);
NK_EXPORT NkOsHandle nk_stdout(void);
NK_EXPORT NkOsHandle nk_stderr(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_FILE_H_
