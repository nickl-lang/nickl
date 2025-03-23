#ifndef NTK_OS_DL_H_
#define NTK_OS_DL_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT extern char const *nkdl_file_extension;

NK_EXPORT NkOsHandle nkdl_loadLibrary(char const *name);
NK_EXPORT void nkdl_freeLibrary(NkOsHandle dl);

NK_EXPORT void *nkdl_resolveSymbol(NkOsHandle dl, char const *name);

NK_EXPORT char const *nkdl_getLastErrorString(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_DL_H_
