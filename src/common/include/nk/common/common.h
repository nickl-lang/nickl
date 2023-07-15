#ifndef HEADER_GUARD_NK_COMMON_COMMON
#define HEADER_GUARD_NK_COMMON_COMMON

#ifdef _WIN32
#define NK_EXPORT __declspec(dllexport)
#else
#define NK_EXPORT __attribute__((__visibility__("default")))
#endif

#endif // HEADER_GUARD_NK_COMMON_COMMON
