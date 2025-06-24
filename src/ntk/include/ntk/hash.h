#ifndef NTK_HASH_H_
#define NTK_HASH_H_

#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u32 parts[4];
} NkHash;

NK_EXPORT NkHash nk_hash(u8 const *buf, usize size);

#define nk_hash_val(val) nk_hash((u8 const *)&(val), sizeof(val))

typedef __attribute__((aligned(8))) struct {
    u8 _[1912];
} NkHashState;

NK_EXPORT void nk_hash_init(NkHashState *state);
NK_EXPORT void nk_hash_update(NkHashState *state, u8 const *buf, usize size);
NK_EXPORT NkHash nk_hash_finalize(NkHashState *state);

#ifdef __cplusplus
}
#endif

#endif // NTK_HASH_H_
