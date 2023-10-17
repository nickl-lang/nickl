#ifndef HEADER_GUARD_NTK_SYS_DL
#define HEADER_GUARD_NTK_SYS_DL

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *nkdl_t;

nkdl_t nk_load_library(char const *name);
void nk_free_library(nkdl_t dl);

void *nk_resolve_symbol(nkdl_t dl, char const *name);

char const *nkdl_getLastErrorString();

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NTK_SYS_DL
