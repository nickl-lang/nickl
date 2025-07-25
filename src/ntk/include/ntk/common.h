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
typedef i64 isize;

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

#define nk_alignofval alignof

#endif // __cplusplus

#if defined(__has_attribute)
#if __has_attribute(unused)
#define NK_UNUSED __attribute__((unused))
#else
#define NK_UNUSED
#endif
#else
#define NK_UNUSED
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define NK_FALLTHROUGH __attribute__((fallthrough))
#else
#define NK_FALLTHROUGH
#endif
#else
#define NK_FALLTHROUGH
#endif

#define _NK_NOP (void)0
#define _NK_NOP_TOPLEVEL extern NK_UNUSED int NK_CAT(_, __LINE__)

#define _NK_EMPTY

#define nk_trap() __builtin_trap()

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    intptr_t val;
} NkHandle;

#define NK_NULL_HANDLE (NK_LITERAL(NkHandle) NK_ZERO_STRUCT)

NK_INLINE bool nk_handleEqual(NkHandle lhs, NkHandle rhs) {
    return lhs.val == rhs.val;
}

NK_INLINE bool nk_handleIsNull(NkHandle handle) {
    return nk_handleEqual(handle, NK_NULL_HANDLE);
}

NK_INLINE void *nk_handleToVoidPtr(NkHandle handle) {
    return (void *)handle.val;
}

NK_INLINE NkHandle nk_handleFromVoidPtr(void *ptr) {
    return NK_LITERAL(NkHandle){(intptr_t)ptr};
}

#ifdef __cplusplus

inline bool operator==(NkHandle lhs, NkHandle rhs) {
    return nk_handleEqual(lhs, rhs);
}

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#define NK_DEFER_LOOP(BEGIN, END) for (int _i_ = ((BEGIN), 0); !_i_; _i_ += 1, (END))
#define NK_DEFER_LOOP_OPT(ENABLE, BEGIN, END) for (int _i_ = !((ENABLE) && ((BEGIN), 1)); !_i_; _i_ += 1, (END))

#define NK_ITERATE(TYPE, IT, SLICE) for (TYPE IT = (SLICE).data; IT < (SLICE).data + (SLICE).size; IT++)
#define NK_INDEX(IT, SLICE) (usize)((IT) - (SLICE).data)

#endif // NTK_COMMON_H_
