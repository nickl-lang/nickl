#include "ntk/hash_tree.h"

#include "ntk/utils.h"

static u64 intptr_t_KeyHash(intptr_t key) {
    return nk_hashVal(key);
}

static bool intptr_t_KeyEqual(intptr_t lhs, intptr_t rhs) {
    return lhs == rhs;
}

NK_HASH_TREE_IMPL_K(NkIntptrHashSet, intptr_t, intptr_t_KeyHash, intptr_t_KeyEqual);
NK_HASH_TREE_IMPL_KV(NkIntptrHashMap, intptr_t, intptr_t, intptr_t_KeyHash, intptr_t_KeyEqual);
