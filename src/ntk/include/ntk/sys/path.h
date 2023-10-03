#ifndef HEADER_GUARD_NTK_SYS_APP
#define HEADER_GUARD_NTK_SYS_APP

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NK_MAX_PATH 4096

int nk_getBinaryPath(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_APP
