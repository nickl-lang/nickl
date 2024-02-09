#ifndef HEADER_GUARD_NTK_COMMON
#define HEADER_GUARD_NTK_COMMON

#include <stdarg.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef size_t usize;

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

#ifdef _WIN32
#define NK_INLINE static inline
#else //_WIN32
#define NK_INLINE inline
#endif //_WIN32

#ifndef CAT
#define _CAT(x, y) x##y
#define CAT(x, y) _CAT(x, y)
#endif // CAT

#define AR_SIZE(AR) (sizeof(AR) / sizeof((AR)[0]))

#ifdef __cplusplus

#define LITERAL(T) T
#define ZERO_STRUCT \
    {}

template <class T>
T *_nk_assign_void_ptr(T *&dst, void *src) {
    return dst = (T *)src;
}

template <class T>
constexpr usize nk_alignofval(T const &) {
    return alignof(T);
}

#else // __cplusplus

#define LITERAL(T) (T)
#define ZERO_STRUCT \
    { 0 }

#define _nk_assign_void_ptr(dst, src) ((dst) = (src))

#define nk_alignofval alignof

#endif // __cplusplus

typedef usize hash_t;

#define _NK_NOP (void)0
#define _NK_NOP_TOPLEVEL extern int CAT(_, __LINE__)

#define NK_FORCEINLINE __attribute__((always_inline))

#endif // HEADER_GUARD_NTK_COMMON
