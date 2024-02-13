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
