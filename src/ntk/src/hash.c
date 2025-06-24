#include "ntk/hash.h"

#include "blake3.h"

// TODO: Move out endianness conversion somewhere
static u32 big2little_endian_32(u32 val) {
    return ((val >> 24) & 0x000000ff) | //
           ((val >> 8) & 0x0000ff00) |  //
           ((val << 8) & 0x00ff0000) |  //
           ((val << 24) & 0xff000000);
}

static NkHash blake3_hash_to_native(u8 hash_buf[sizeof(NkHash)]) {
    NkHash hash;
    hash.parts[0] = big2little_endian_32(*(u32 *)&hash_buf[0]);
    hash.parts[1] = big2little_endian_32(*(u32 *)&hash_buf[4]);
    hash.parts[2] = big2little_endian_32(*(u32 *)&hash_buf[8]);
    hash.parts[3] = big2little_endian_32(*(u32 *)&hash_buf[12]);
    return hash;
}

NkHash nk_hash(u8 const *buf, usize size) {
    NkHashState state;
    nk_hash_init(&state);
    nk_hash_update(&state, buf, size);
    return nk_hash_finalize(&state);
}

_Static_assert(sizeof(NkHashState) == sizeof(blake3_hasher), "invalid hash state struct");
_Static_assert(alignof(NkHashState) == alignof(blake3_hasher), "invalid hash state struct");

void nk_hash_init(NkHashState *state) {
    blake3_hasher_init((blake3_hasher *)state);
}

void nk_hash_update(NkHashState *state, u8 const *buf, usize size) {
    blake3_hasher_update((blake3_hasher *)state, buf, size);
}

NkHash nk_hash_finalize(NkHashState *state) {
    u8 hash_buf[sizeof(NkHash)];
    blake3_hasher_finalize((blake3_hasher *)state, hash_buf, sizeof(hash_buf));

    return blake3_hash_to_native(hash_buf);
}
