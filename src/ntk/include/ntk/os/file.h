#ifndef NTK_OS_FILE_H_
#define NTK_OS_FILE_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkfd_t;

extern nkfd_t nk_invalid_fd;
extern char const *nk_null_file;

i32 nk_read(nkfd_t fd, char *buf, usize n);
i32 nk_write(nkfd_t fd, char const *buf, usize n);

typedef enum {
    NkOpenFlags_Read = 1,
    NkOpenFlags_Write = 2,
    NkOpenFlags_Create = 4,
    NkOpenFlags_Truncate = 8,
} NkOpenFlags;

nkfd_t nk_open(char const *file, i32 flags);

i32 nk_close(nkfd_t fd);

nkfd_t nk_stdin();
nkfd_t nk_stdout();
nkfd_t nk_stderr();

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_FILE_H_
