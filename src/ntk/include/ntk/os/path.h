#ifndef NTK_OS_APP_H_
#define NTK_OS_APP_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NK_MAX_PATH 4096

extern char nk_path_separator;

i32 nk_getBinaryPath(char *buf, usize size);

#ifdef __cplusplus
}
#endif

#endif // NTK_OS_APP_H_
