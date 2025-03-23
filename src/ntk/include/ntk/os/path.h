#ifndef NTK_OS_APP_H_
#define NTK_OS_APP_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)

#define NK_MAX_PATH 260
#define NK_PATH_SEPARATOR '\\'

#elif defined(__APPLE__)

#define NK_MAX_PATH 1024
#define NK_PATH_SEPARATOR '/'

#else

#define NK_MAX_PATH 4096
#define NK_PATH_SEPARATOR '/'

#endif

NK_EXPORT i32 nk_getBinaryPath(char *buf, usize size);

NK_EXPORT i32 nk_fullPath(char *buf, char const *path);
NK_EXPORT i32 nk_getCwd(char *buf, usize size);

NK_EXPORT bool nk_pathIsRelative(char const *path);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_APP_H_
