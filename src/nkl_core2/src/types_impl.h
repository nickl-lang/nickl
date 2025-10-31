#ifndef NKL_CORE_TYPES_IMPL_H_
#define NKL_CORE_TYPES_IMPL_H_

#include "nkl/core/types.h"
#include "ntk/arena.h"
#include "ntk/hash.h"
#include "ntk/hash_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_HASH_TREE_FWD_KV(NklTypeMap, NkHash128, NklType);

typedef struct {
    NkArena *arena;
    NklTypeMap map;
    NklTypeClass next_tclass;
} NklTypeStorage;

void nkl_types_init(NklTypeStorage *st, NkArena *arena);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_IMPL_H_
