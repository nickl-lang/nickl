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

typedef enum {
    nk_open_read = 1 << 0,
    nk_open_write = 1 << 1,
    nk_open_create = 1 << 2,
    nk_open_truncate = 1 << 3,
} nk_open_flags;

nkfd_t nk_open(char const *file, nk_open_flags flags);

int nk_close(nkfd_t fd);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_FILE
