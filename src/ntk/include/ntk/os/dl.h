#ifndef NTK_OS_DL_H_
#define NTK_OS_DL_H_

#include "ntk/os/common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char const *nkdl_file_extension;

NkOsHandle nkdl_loadLibrary(char const *name);
void nkdl_freeLibrary(NkOsHandle dl);

void *nkdl_resolveSymbol(NkOsHandle dl, char const *name);

char const *nkdl_getLastErrorString(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_DL_H_
