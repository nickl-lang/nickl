#include "ntk/utils.h"

#define HASH_ARRAY_STEP sizeof(usize)

u64 nk_hashArray(u8 const *begin, u8 const *end) {
    u64 hash = 0;
    usize i = 0;
    while (begin + (i + 1) * HASH_ARRAY_STEP <= end) {
        nk_hashCombine(&hash, *(usize const *)(begin + i++ * HASH_ARRAY_STEP));
    }
    i *= HASH_ARRAY_STEP;
    while (begin + i < end) {
        nk_hashCombine(&hash, begin[i++]);
    }
    return hash;
}

extern inline usize nk_roundUp(usize v, usize m);
extern inline usize nk_roundUpSafe(usize v, usize m);
extern inline u64 nk_ceilToPowerOf2(u64 n);
extern inline u64 nk_floorToPowerOf2(u64 n);
extern inline bool nk_isZeroOrPowerOf2(u64 n);
extern inline u64 nk_log2u64(u64 n);
extern inline u32 nk_log2u32(u32 n);
extern inline u64 nk_minu(u64 x, u64 y);
extern inline u64 nk_maxu(u64 x, u64 y);
extern inline i64 nk_mini(i64 x, i64 y);
extern inline i64 nk_maxi(i64 x, i64 y);
extern inline void nk_hashCombine(u64 *seed, usize n);
extern inline u64 nk_hashCStr(char const *str);
