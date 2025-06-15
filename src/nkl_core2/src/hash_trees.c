#include "hash_trees.h"

NK_HASH_TREE_IMPL_K(NkAtomHashSet, NkAtom, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkAtomHashMap, NkAtom, NkAtom, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkAtomStringHashMap, NkAtom, NkString, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkAtomModuleHashMap, NkAtom, NklModule, nk_atom_hash, nk_atom_equal);
