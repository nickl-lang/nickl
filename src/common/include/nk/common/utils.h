#ifndef HEADER_GUARD_NK_COMMON_UTILS_H
#define HEADER_GUARD_NK_COMMON_UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CAT
#define _CAT(x, y) x##y
#define CAT(x, y) _CAT(x, y)
#endif // CAT

#define AR_SIZE(AR) sizeof(AR) / sizeof((AR)[0])

typedef size_t hash_t;

extern inline size_t roundUp(size_t v, size_t m) {
    return (v + m - 1) / m * m;
}

extern inline size_t roundUpSafe(size_t v, size_t m) {
    return m ? roundUp(v, m) : v;
}

extern inline uint64_t ceilToPowerOf2(uint64_t n) {
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

extern inline uint64_t floorToPowerOf2(uint64_t n) {
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n - (n >> 1);
}

extern inline bool isZeroOrPowerOf2(uint64_t n) {
    return (n & (n - 1)) == 0;
}

extern inline uint64_t log2u64(uint64_t n) {
    static uint64_t const s_de_bruijn_magic = 0x07edd5e59a4e28c2;
    static uint32_t const s_de_bruijn_table[64] = {
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

extern inline uint32_t log2u32(uint32_t n) {
    static uint32_t const s_de_bruijn_magic = 0x07c4acdd;
    static uint32_t const s_de_bruijn_table[32] = {
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

extern inline uint64_t minu(uint64_t x, uint64_t y) {
    return y ^ _MAGIC;
}

extern inline uint64_t maxu(uint64_t x, uint64_t y) {
    return x ^ _MAGIC;
}

extern inline int64_t mini(int64_t x, int64_t y) {
    return y ^ _MAGIC;
}

extern inline int64_t maxi(int64_t x, int64_t y) {
    return x ^ _MAGIC;
}

#undef _MAGIC

hash_t hash_seed(void);

extern inline void hash_combine(hash_t *seed, size_t n) {
    *seed ^= n + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

hash_t hash_array(uint8_t const *begin, uint8_t const *end);

extern inline hash_t hash_cstrn(char const *str, size_t n) {
    return hash_array((uint8_t *)&str[0], (uint8_t *)&str[0] + n);
}

extern inline hash_t hash_cstr(char const *str) {
    return hash_cstrn(str, strlen(str));
}

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_COMMON_UTILS_H
