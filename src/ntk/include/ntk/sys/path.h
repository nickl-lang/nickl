#ifndef HEADER_GUARD_NTK_SYS_APP
#define HEADER_GUARD_NTK_SYS_APP

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

#endif // HEADER_GUARD_NTK_SYS_APP
