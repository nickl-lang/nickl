#ifndef NTK_DL_H_
#define NTK_DL_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT extern char const *nkdl_file_extension;

NK_EXPORT NkHandle nkdl_loadLibrary(char const *name);
NK_EXPORT void nkdl_freeLibrary(NkHandle dl);

NK_EXPORT void *nkdl_resolveSymbol(NkHandle dl, char const *name);

NK_EXPORT char const *nkdl_getLastErrorString(void);

#ifdef __cplusplus
}
#endif

#endif // NTK_DL_H_
