#ifndef HEADER_GUARD_NK_COMMON_UTILS
#define HEADER_GUARD_NK_COMMON_UTILS

#include <cstdarg>
#include <cstdbool>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "nk/common/common.hpp"
#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"

#define _CAT(x, y) x##y
#define CAT(x, y) _CAT(x, y)
#define DEFER(BLOCK) std::shared_ptr<void> CAT(__defer_, __LINE__){nullptr, [&](void *) BLOCK};

inline size_t roundUp(size_t v, size_t m) {
    return (v + m - 1) / m * m;
}

inline size_t roundUpSafe(size_t v, size_t m) {
    return m ? roundUp(v, m) : v;
}

inline uint64_t ceilToPowerOf2(uint64_t n) {
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

inline uint64_t floorToPowerOf2(uint64_t n) {
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n - (n >> 1);
}

inline bool isZeroOrPowerOf2(uint64_t n) {
    return (n & (n - 1)) == 0;
}

inline uint32_t log2u(uint32_t n) {
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

inline uint64_t minu(uint64_t x, uint64_t y) {
    return y ^ _MAGIC;
}

inline uint64_t maxu(uint64_t x, uint64_t y) {
    return x ^ _MAGIC;
}

inline int64_t mini(int64_t x, int64_t y) {
    return y ^ _MAGIC;
}

inline int64_t maxi(int64_t x, int64_t y) {
    return x ^ _MAGIC;
}

#undef _MAGIC

hash_t hash_seed(void);

inline void hash_combine(hash_t *seed, size_t n) {
    *seed ^= n + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
}

hash_t hash_array(uint8_t const *begin, uint8_t const *end);

inline hash_t hash_strn(char const *str, size_t n) {
    return hash_array((uint8_t *)&str[0], (uint8_t *)&str[0] + n);
}

inline hash_t hash_str(char const *str) {
    return hash_strn(str, strlen(str));
}

string string_format(Allocator *allocator, char const *fmt, ...);
string string_vformat(Allocator *allocator, char const *fmt, va_list ap);

template <class THeader, class TElem>
size_t arrayWithHeaderSize(size_t n) {
    return roundUp(sizeof(THeader), alignof(TElem)) + sizeof(TElem) * n;
}

template <class THeader, class TElem>
TElem *arrayWithHeaderData(THeader *header) {
    return (TElem *)roundUp((size_t)(header + 1), alignof(TElem));
}

#endif // HEADER_GUARD_NK_COMMON_UTILS
