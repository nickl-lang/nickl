#ifndef NTK_OS_FILE_H_
#define NTK_OS_FILE_H_

#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char const *nk_null_file;

i32 nk_read(NkOsHandle fd, char *buf, usize n);
i32 nk_write(NkOsHandle fd, char const *buf, usize n);

typedef enum {
    NkOpenFlags_Read = 1,
    NkOpenFlags_Write = 2,
    NkOpenFlags_Create = 4,
    NkOpenFlags_Truncate = 8,
} NkOpenFlags;

NkOsHandle nk_open(char const *file, i32 flags);

i32 nk_close(NkOsHandle fd);

NkOsHandle nk_stdin();
NkOsHandle nk_stdout();
NkOsHandle nk_stderr();

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_FILE_H_
