#ifndef NTK_OS_APP_H_
#define NTK_OS_APP_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define NK_MAX_PATH 260
#else //_WIN32
#define NK_MAX_PATH 4096
#endif //_WIN32

extern char nk_path_separator;

i32 nk_getBinaryPath(char *buf, usize size);

i32 nk_fullPath(char *buf, char const *path);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_APP_H_
