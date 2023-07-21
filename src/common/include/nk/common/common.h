#ifndef HEADER_GUARD_NK_COMMON_COMMON
#define HEADER_GUARD_NK_COMMON_COMMON

#ifdef _WIN32
#define NK_EXPORT __declspec(dllexport)
#else
#define NK_EXPORT __attribute__((__visibility__("default")))
#endif

#define NK_PRINTF_LIKE(FMT_POS, ARGS_POS) __attribute__((format(printf, FMT_POS, ARGS_POS)))

#endif // HEADER_GUARD_NK_COMMON_COMMON
