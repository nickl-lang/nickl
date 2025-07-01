#include "types.h"

#include "ntk/hash.h"
#include "ntk/hash_tree.h"

// TODO: Use 128bit hash directly?
NkHash64 NkHash128_KeyHash(NkHash128 key) {
    return *(NkHash64 *)key.bytes;
}

// NkHash128 const *NklType_T_GetKey(NklType item) {
//     return &item->key;
// }

NK_HASH_TREE_IMPL_KV(NklTypeMap, NkHash128, NklType, NkHash128_KeyHash, nk_hash128_equal);
// NK_HASH_TREE_IMPL(NklTypeMap, NklType_T, NkHash128, NklType_T_GetKey, NkHash128_KeyHash, nk_hash128_equal);
