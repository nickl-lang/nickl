#include "ntk/utils.h"

#define HASH_ARRAY_STEP sizeof(size_t)

hash_t hash_array(uint8_t const *begin, uint8_t const *end) {
    hash_t hash = 0;
    size_t i = 0;
    while (begin + (i + 1) * HASH_ARRAY_STEP <= end) {
        hash_combine(&hash, *(size_t const *)(begin + i++ * HASH_ARRAY_STEP));
    }
    i *= HASH_ARRAY_STEP;
    while (begin + i < end) {
        hash_combine(&hash, begin[i++]);
    }
    return hash;
}

extern inline size_t roundUp(size_t v, size_t m);
extern inline size_t roundUpSafe(size_t v, size_t m);
extern inline uint64_t ceilToPowerOf2(uint64_t n);
extern inline uint64_t floorToPowerOf2(uint64_t n);
extern inline bool isZeroOrPowerOf2(uint64_t n);
extern inline uint64_t log2u64(uint64_t n);
extern inline uint32_t log2u32(uint32_t n);
extern inline uint64_t minu(uint64_t x, uint64_t y);
extern inline uint64_t maxu(uint64_t x, uint64_t y);
extern inline int64_t mini(int64_t x, int64_t y);
extern inline int64_t maxi(int64_t x, int64_t y);
extern inline void hash_combine(hash_t *seed, size_t n);
extern inline hash_t hash_cstrn(char const *str, size_t n);
extern inline hash_t hash_cstr(char const *str);
