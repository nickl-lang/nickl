#ifndef NTK_HASH_H_
#define NTK_HASH_H_

#include <string.h>

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u64 NkHash64;

NK_EXPORT NkHash64 nk_hash64(u8 const *buf, usize size);

#define nk_hash64_val(val) nk_hash64((u8 const *)&(val), sizeof(val))

typedef struct {
    u8 bytes[16];
} NkHash128;

typedef struct {
    u64 _[239];
} NkHashState;

NK_EXPORT void nk_hash128_init(NkHashState *state);
NK_EXPORT void nk_hash128_update(NkHashState *state, u8 const *buf, usize size);
NK_EXPORT NkHash128 nk_hash128_finalize(NkHashState *state);

NK_INLINE NkHash128 nk_hash128(u8 const *buf, usize size) {
    NkHashState state;
    nk_hash128_init(&state);
    nk_hash128_update(&state, buf, size);
    return nk_hash128_finalize(&state);
}

NK_INLINE int nk_hash128_cmp(NkHash128 lhs, NkHash128 rhs) {
    return memcmp(&lhs, &rhs, sizeof(NkHash128));
}

NK_INLINE bool nk_hash128_equal(NkHash128 lhs, NkHash128 rhs) {
    return nk_hash128_cmp(lhs, rhs) == 0;
}

NK_INLINE bool nk_hash128_less(NkHash128 lhs, NkHash128 rhs) {
    return nk_hash128_cmp(lhs, rhs) < 0;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_HASH_H_
