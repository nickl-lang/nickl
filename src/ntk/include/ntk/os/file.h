#ifndef NTK_OS_FILE_H_
#define NTK_OS_FILE_H_

#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#endif // NTK_OS_FILE_H_
