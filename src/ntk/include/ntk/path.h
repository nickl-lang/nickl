#ifndef NTK_PATH_
#define NTK_PATH_

#include "ntk/os/path.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_relativePath(char *buf, usize size, char const *full_path, char const *full_base);

NkString nk_path_getParent(NkString full_path);

#ifdef __cplusplus
}
#endif

#endif // NTK_PATH_
