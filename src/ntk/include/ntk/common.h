#ifndef NTK_COMMON_H_
#define NTK_COMMON_H_

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
#define NK_PRINTF_LIKE(FMT_POS) __attribute__((__format__(__MINGW_PRINTF_FORMAT, FMT_POS, FMT_POS + 1)))
#else //__MINGW32__
#define NK_PRINTF_LIKE(FMT_POS) __attribute__((__format__(printf, FMT_POS, FMT_POS + 1)))
#endif //__MINGW32__

#define NK_INLINE static inline

#define NK_FORCEINLINE __attribute__((always_inline))

#define _NK_CAT(x, y) x##y
#define NK_CAT(x, y) _NK_CAT(x, y)

#define _NK_STRINGIFY(x) #x
#define NK_STRINGIFY(x) _NK_STRINGIFY(x)

#define NK_ARRAY_COUNT(AR) (sizeof(AR) / sizeof((AR)[0]))

#ifdef __cplusplus

#define NK_LITERAL(T) T
#define NK_ZERO_STRUCT \
    {                  \
    }

template <class T>
T *_nk_assignVoidPtr(T *&dst, void *src) {
    return dst = (T *)src;
}

template <class T>
constexpr usize nk_alignofval(T const &) {
    return alignof(T);
}

#else // __cplusplus

#define NK_LITERAL(T) (T)
#define NK_ZERO_STRUCT {0}

#define _nk_assignVoidPtr(dst, src) ((dst) = (src))

#define nk_alignofval(X) alignof(typeof(X))

#endif // __cplusplus

#define _NK_NOP (void)0
#define _NK_NOP_TOPLEVEL extern int NK_CAT(_, __LINE__)

#define nk_trap() __builtin_trap()

#ifdef NDEBUG
#define nk_assert(x) (void)(x)
#else // NDEBUG
// TODO nk_assert depends on libc
#define nk_assert(x)                                                                             \
    do {                                                                                         \
        if (!(x)) {                                                                              \
            fprintf(stderr, __FILE__ ":" NK_STRINGIFY(__LINE__) ": Assertion failed: " #x "\n"); \
            fflush(stderr);                                                                      \
            nk_trap();                                                                           \
        }                                                                                        \
    } while (0)
#endif // NDEBUG

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    intptr_t val;
} NkOsHandle;

#define NK_OS_HANDLE_ZERO (NK_LITERAL(NkOsHandle) NK_ZERO_STRUCT)

NK_INLINE bool nkos_handleEqual(NkOsHandle lhs, NkOsHandle rhs) {
    return lhs.val == rhs.val;
}

NK_INLINE bool nkos_handleIsZero(NkOsHandle handle) {
    return nkos_handleEqual(handle, NK_OS_HANDLE_ZERO);
}

NK_INLINE void *nkos_handleToVoidPtr(NkOsHandle handle) {
    return (void *)handle.val;
}

NK_INLINE NkOsHandle nkos_handleFromVoidPtr(void *ptr) {
    return NK_LITERAL(NkOsHandle){(intptr_t)ptr};
}

#ifdef __cplusplus

inline bool operator==(NkOsHandle lhs, NkOsHandle rhs) {
    return nkos_handleEqual(lhs, rhs);
}

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // NTK_COMMON_H_
