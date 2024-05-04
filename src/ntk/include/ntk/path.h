#ifndef NTK_PATH_
#define NTK_PATH_

#include "ntk/os/path.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_relativePath(char *buf, usize size, char const *full_path, char const *full_base);

#ifdef __cplusplus
}
#endif

#endif // NTK_PATH_
