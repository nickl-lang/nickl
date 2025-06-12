#include "ntk/hash_tree.h"

#include "ntk/utils.h"

static u64 intrptr_hash(intptr_t key) {
    return nk_hashVal(key);
}

static bool intrptr_equal(intptr_t lhs, intptr_t rhs) {
    return lhs == rhs;
}

static intptr_t const *NkIntptr_kv_getKey(NkIntptr_kv const *item) {
    return &item->key;
}

NK_HASH_TREE_IMPL(NkIntptrHashTree, NkIntptr_kv, intptr_t, NkIntptr_kv_getKey, intrptr_hash, intrptr_equal);
