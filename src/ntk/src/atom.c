#include "ntk/atom.h"

#include "ntk/arena.h"
#include "ntk/hash_tree.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

NK_HASH_TREE_IMPL_K(NkAtomSet, NkAtom, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkAtomMap, NkAtom, NkAtom, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkAtomStringMap, NkAtom, NkString, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL_KV(NkStringAtomMap, NkString, NkAtom, nks_hash, nks_equal);

static NkArena g_arena;
static NkStringAtomMap g_str2atom;
static NkAtomStringMap g_atom2str;
static NkAtom g_next_atom = 1000;

void nk_atom_init(void) {
    NkAllocator alloc = nk_arena_getAllocator(&g_arena);
    g_str2atom.alloc = alloc;
    g_atom2str.alloc = alloc;
}

void nk_atom_deinit(void) {
}

NkString nk_atom2s(NkAtom atom) {
    NkString ret;
    NK_PROF_FUNC() {
        NkString const *found = NkAtomStringMap_find(&g_atom2str, atom);
        ret = found ? *found : (NkString){0};
    }
    return ret;
}

char const *nk_atom2cs(NkAtom atom) {
    return nk_atom2s(atom).data;
}

NkAtom nk_s2atom(NkString str) {
    NkAtom ret = 0;
    NK_PROF_FUNC() {
        NkAtom const *found = NkStringAtomMap_find(&g_str2atom, str);

        if (found) {
            ret = *found;
        } else {
            NkAtom atom = g_next_atom++;
            nk_atom_define(atom, str);
            ret = atom;
        }
    }
    return ret;
}

NkAtom nk_cs2atom(char const *str) {
    return nk_s2atom(nk_cs2s(str));
}

void nk_atom_define(NkAtom atom, NkString str) {
    NK_PROF_FUNC() {
        NkString const str_copy = nks_copyNt(nk_arena_getAllocator(&g_arena), str);
        NkStringAtomMap_insert(&g_str2atom, str_copy, atom);
        NkAtomStringMap_insert(&g_atom2str, atom, str_copy);
    }
}

NkAtom nk_atom_unique(NkString str) {
    NkAtom atom = g_next_atom++;
    NK_PROF_FUNC() {
        NkString const str_copy = nks_copyNt(nk_arena_getAllocator(&g_arena), str);
        NkAtomStringMap_insert(&g_atom2str, atom, str_copy);
    }
    return atom;
}
