#ifndef NTK_UTILS_H_
#define NTK_UTILS_H_

#include <string.h>

#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/stream.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_INLINE usize nk_roundUp(usize v, usize m) {
    return (v + m - 1) / m * m;
}

NK_INLINE usize nk_roundUpSafe(usize v, usize m) {
    return m ? nk_roundUp(v, m) : v;
}

NK_INLINE u64 nk_ceilToPowerOf2(u64 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

NK_INLINE u64 nk_floorToPowerOf2(u64 n) {
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n - (n >> 1);
}

NK_INLINE bool nk_isZeroOrPowerOf2(u64 n) {
    return (n & (n - 1)) == 0;
}

NK_INLINE u64 nk_log2u64(u64 n) {
    static u64 const s_de_bruijn_magic = 0x07edd5e59a4e28c2;
    static u32 const s_de_bruijn_table[64] = {
        63, 0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,  61, 51, 37, 40, 49, 18,
        28, 20, 55, 30, 34, 11, 43, 14, 22, 4,  62, 57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19,
        29, 10, 13, 21, 56, 45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5,
    };

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;

    return s_de_bruijn_table[(((n - (n >> 1)) * s_de_bruijn_magic)) >> 58];
}

NK_INLINE u32 nk_log2u32(u32 n) {
    static u32 const s_de_bruijn_magic = 0x07c4acdd;
    static u32 const s_de_bruijn_table[32] = {
        0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31,
    };

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return s_de_bruijn_table[(n * s_de_bruijn_magic) >> 27];
}

#define _MAGIC ((x ^ y) & -(x < y))

NK_INLINE u64 nk_minu(u64 x, u64 y) {
    return y ^ _MAGIC;
}

NK_INLINE u64 nk_maxu(u64 x, u64 y) {
    return x ^ _MAGIC;
}

NK_INLINE i64 nk_mini(i64 x, i64 y) {
    return y ^ _MAGIC;
}

NK_INLINE i64 nk_maxi(i64 x, i64 y) {
    return x ^ _MAGIC;
}

#undef _MAGIC

NK_INLINE void nk_hashCombine(u64 *seed, usize n) {
    *seed ^= n + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

NK_EXPORT u64 nk_hashArray(u8 const *begin, u8 const *end);

#define nk_hashVal(val) nk_hashArray((u8 const *)&(val), (u8 const *)(&(val) + 1))

NK_INLINE u64 nk_hashCStr(char const *str) {
    return nk_hashArray((u8 *)str, (u8 *)str + strlen(str));
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <utility>

template <class F>
struct [[nodiscard]] _NkDefer {
    F f;
    ~_NkDefer() {
        f();
    }
};

template <class T, class F>
struct [[nodiscard]] _NkDeferWithData {
    T data;
    F f;
    operator T() const {
        return data;
    }
    ~_NkDeferWithData() {
        f();
    }
};

#ifndef defer
struct _NkDeferDummy {};
template <class F>
_NkDefer<F> operator*(_NkDeferDummy, F &&f) {
    return {std::forward<F>(f)};
}
#define defer auto NK_CAT(__defer, __LINE__) = _NkDeferDummy{} *[&]()
#endif // defer

template <class F>
_NkDefer<F> nk_defer(F &&f) {
    return {std::forward<F>(f)};
}

template <class T, class F>
_NkDeferWithData<T, F> nk_defer(T &&data, F &&f) {
    return {std::forward<T>(data), std::forward<F>(f)};
}

#endif // __cplusplus

#ifdef NDEBUG
#define nk_assert(x) (void)(x)
#else // NDEBUG
#define nk_assert(x)                                                                                                 \
    do {                                                                                                             \
        if (!(x)) {                                                                                                  \
            nk_printf(                                                                                               \
                nk_file_getStream(nk_stderr()), __FILE__ ":" NK_STRINGIFY(__LINE__) ": Assertion failed: " #x "\n"); \
            nk_trap();                                                                                               \
        }                                                                                                            \
    } while (0)
#endif // NDEBUG

#endif // NTK_UTILS_H_
