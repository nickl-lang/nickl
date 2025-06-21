#ifndef NTK_PATH_
#define NTK_PATH_

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Rewrite to use NkString
NK_EXPORT i32 nk_relativePath(char *buf, usize size, char const *full_path, char const *full_base);

NK_EXPORT i32 nk_getTempPath(char *buf, usize size);

NK_EXPORT NkString nk_path_getParent(NkString path);
NK_EXPORT NkString nk_path_getFilename(NkString path);
NK_EXPORT NkString nk_path_getExtension(NkString path);

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

#endif // NTK_PATH_
