#ifndef NKL_CORE_HASH_TREES_H_
#define NKL_CORE_HASH_TREES_H_

#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/hash_tree.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_HASH_TREE_FWD_K(NkAtomHashSet, NkAtom);
NK_HASH_TREE_FWD_KV(NkAtomHashMap, NkAtom, NkAtom);
NK_HASH_TREE_FWD_KV(NkAtomStringHashMap, NkAtom, NkString);
NK_HASH_TREE_FWD_KV(NkAtomModuleHashMap, NkAtom, NklModule);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_HASH_TREES_H_
