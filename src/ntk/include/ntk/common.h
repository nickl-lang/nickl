#ifndef HEADER_GUARD_NTK_COMMON
#define HEADER_GUARD_NTK_COMMON

#include <inttypes.h>
#include <stdio.h>

#ifdef _WIN32
#define NK_EXPORT __declspec(dllexport)
#else //_WIN32
#define NK_EXPORT __attribute__((__visibility__("default")))
#endif //_WIN32

#ifdef __MINGW32__
#define NK_PRINTF_LIKE(FMT_POS, ARGS_POS) __attribute__((__format__(__MINGW_PRINTF_FORMAT, FMT_POS, ARGS_POS)))
#else //__MINGW32__
#define NK_PRINTF_LIKE(FMT_POS, ARGS_POS) __attribute__((__format__(printf, FMT_POS, ARGS_POS)))
#endif //__MINGW32__

#ifdef __cplusplus
#define LITERAL(T) T
#else // __cplusplus
#define LITERAL(T) (T)
#endif // __cplusplus

#ifdef _WIN32
#define NK_INLINE static inline
#else //_WIN32
#define NK_INLINE inline
#endif //_WIN32

#endif // HEADER_GUARD_NTK_COMMON
