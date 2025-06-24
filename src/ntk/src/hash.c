#include "ntk/hash.h"

#include <assert.h>

#include "blake3.h"
#include "rapidhash.h"

NkHash64 nk_hash64(u8 const *buf, usize size) {
    return rapidhash(buf, size);
}

static_assert(sizeof(NkHashState) == sizeof(blake3_hasher), "invalid hash state struct");
static_assert(alignof(NkHashState) == alignof(blake3_hasher), "invalid hash state struct");

void nk_hash128_init(NkHashState *state) {
    blake3_hasher_init((blake3_hasher *)state);
}

void nk_hash128_update(NkHashState *state, u8 const *buf, usize size) {
    blake3_hasher_update((blake3_hasher *)state, buf, size);
}

NkHash128 nk_hash128_finalize(NkHashState *state) {
    NkHash128 hash;
    blake3_hasher_finalize((blake3_hasher *)state, hash.bytes, sizeof(hash.bytes));
    return hash;
}
