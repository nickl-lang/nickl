#ifndef HEADER_GUARD_NTK_SYS_FILE
#define HEADER_GUARD_NTK_SYS_FILE

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkfd_t;

extern nkfd_t nk_invalid_fd;
extern char const *nk_null_file;

i32 nk_read(nkfd_t fd, char *buf, usize n);
i32 nk_write(nkfd_t fd, char const *buf, usize n);

#define nk_open_read 1
#define nk_open_write 2
#define nk_open_create 4
#define nk_open_truncate 8

nkfd_t nk_open(char const *file, i32 flags);

i32 nk_close(nkfd_t fd);

nkfd_t nk_stdin();
nkfd_t nk_stdout();
nkfd_t nk_stderr();

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_FILE
