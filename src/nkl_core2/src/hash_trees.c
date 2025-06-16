#include "hash_trees.h"

NK_HASH_TREE_IMPL_KV(NkAtomModuleMap, NkAtom, NklModule, nk_atom_hash, nk_atom_equal);
