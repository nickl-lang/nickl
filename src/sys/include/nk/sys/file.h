#ifndef HEADER_GUARD_NK_SYS_FILE
#define HEADER_GUARD_NK_SYS_FILE

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t nkfd_t;

nkfd_t nk_invalidFd(void);

int nk_read(nkfd_t fd, char *buf, size_t n);
int nk_write(nkfd_t fd, char const *buf, size_t n);

nkfd_t nk_openNullFile(void);

int nk_close(nkfd_t fd);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_SYS_FILE
