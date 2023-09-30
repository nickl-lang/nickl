#ifndef HEADER_GUARD_NK_SYS_FILE
#define HEADER_GUARD_NK_SYS_FILE

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkfd_t;

extern nkfd_t nk_invalid_fd;
extern char const *nk_null_file;

int nk_read(nkfd_t fd, char *buf, size_t n);
int nk_write(nkfd_t fd, char const *buf, size_t n);

#define nk_open_read 1
#define nk_open_write 2
#define nk_open_create 4
#define nk_open_truncate 8

nkfd_t nk_open(char const *file, int flags);

int nk_close(nkfd_t fd);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_FILE
