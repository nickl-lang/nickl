#include "ntk/utils.h"

#define HASH_ARRAY_STEP sizeof(usize)

hash_t hash_array(u8 const *begin, u8 const *end) {
    hash_t hash = 0;
    usize i = 0;
    while (begin + (i + 1) * HASH_ARRAY_STEP <= end) {
        hash_combine(&hash, *(usize const *)(begin + i++ * HASH_ARRAY_STEP));
    }
    i *= HASH_ARRAY_STEP;
    while (begin + i < end) {
        hash_combine(&hash, begin[i++]);
    }
    return hash;
}

extern inline usize roundUp(usize v, usize m);
extern inline usize roundUpSafe(usize v, usize m);
extern inline u64 ceilToPowerOf2(u64 n);
extern inline u64 floorToPowerOf2(u64 n);
extern inline bool isZeroOrPowerOf2(u64 n);
extern inline u64 log2u64(u64 n);
extern inline u32 log2u32(u32 n);
extern inline u64 minu(u64 x, u64 y);
extern inline u64 maxu(u64 x, u64 y);
extern inline i64 mini(i64 x, i64 y);
extern inline i64 maxi(i64 x, i64 y);
extern inline void hash_combine(hash_t *seed, usize n);
extern inline hash_t hash_cstrn(char const *str, usize n);
extern inline hash_t hash_cstr(char const *str);
