#ifndef HEADER_GUARD_NK_COMMON_COMMON
#define HEADER_GUARD_NK_COMMON_COMMON

#include "inttypes.h"
#include "stdio.h"

#ifdef _WIN32
#define NK_EXPORT __declspec(dllexport)
#else
#define NK_EXPORT __attribute__((__visibility__("default")))
#endif

#ifdef __MINGW32__
#define NK_PRINTF_LIKE(FMT_POS, ARGS_POS) __attribute__((__format__(__MINGW_PRINTF_FORMAT, FMT_POS, ARGS_POS)))
#else
#define NK_PRINTF_LIKE(FMT_POS, ARGS_POS) __attribute__((__format__(printf, FMT_POS, ARGS_POS)))
#endif

#ifdef __cplusplus
#define LITERAL(T) T
#else // __cplusplus
#define LITERAL(T) (T)
#endif // __cplusplus

#endif // HEADER_GUARD_NK_COMMON_COMMON
