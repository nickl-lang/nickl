#ifndef NTK_OS_APP_H_
#define NTK_OS_APP_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32

#define NK_MAX_PATH 260
#define NK_PATH_SEPARATOR '\\'

#else //_WIN32

#define NK_MAX_PATH 4096
#define NK_PATH_SEPARATOR '/'

#endif //_WIN32

i32 nk_getBinaryPath(char *buf, usize size);

i32 nk_fullPath(char *buf, char const *path);
i32 nk_getCwd(char *buf, usize size);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_APP_H_
