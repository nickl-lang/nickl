#include "ntk/atom.h"

#include "ntk/arena.h"
#include "ntk/hash_tree.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

typedef struct {
    NkString key;
    NkAtom val;
} s2atom_kv;

typedef struct {
    NkAtom key;
    NkString val;
} atom2s_kv;

static NkString const *s2atom_kv_GetKey(s2atom_kv const *item) {
    return &item->key;
}

static NkAtom const *atom2s_kv_GetKey(atom2s_kv const *item) {
    return &item->key;
}

static u64 nk_atom_hash(NkAtom const key) {
    return nk_hashVal(key);
}

static bool nk_atom_equal(NkAtom const lhs, NkAtom const rhs) {
    return lhs == rhs;
}

NK_HASH_TREE_DEFINE(str2atom, s2atom_kv, NkString, s2atom_kv_GetKey, nks_hash, nks_equal);
NK_HASH_TREE_DEFINE(atom2str, atom2s_kv, NkAtom, atom2s_kv_GetKey, nk_atom_hash, nk_atom_equal);

static NkArena g_arena;
static str2atom g_str2atom;
static atom2str g_atom2str;
static NkAtom g_next_atom = 1000;

NkString nk_atom2s(NkAtom atom) {
    NK_PROF_FUNC_BEGIN();
    atom2s_kv const *found = atom2str_find(&g_atom2str, atom);
    NK_PROF_FUNC_END();
    return found ? found->val : (NkString){0};
}

char const *nk_atom2cs(NkAtom atom) {
    return nk_atom2s(atom).data;
}

NkAtom nk_s2atom(NkString str) {
    NK_PROF_FUNC_BEGIN();

    s2atom_kv const *found = str2atom_find(&g_str2atom, str);

    if (found) {
        NK_PROF_FUNC_END();
        return found->val;
    } else {
        NkAtom atom = g_next_atom++;
        nk_atom_define(atom, str);
        NK_PROF_FUNC_END();
        return atom;
    }
}

NkAtom nk_cs2atom(char const *str) {
    return nk_s2atom(nk_cs2s(str));
}

void nk_atom_define(NkAtom atom, NkString str) {
    NK_PROF_FUNC_BEGIN();

    NkString const str_copy = nks_copyNt(nk_arena_getAllocator(&g_arena), str);
    str2atom_insert(&g_str2atom, (s2atom_kv){str_copy, atom});
    atom2str_insert(&g_atom2str, (atom2s_kv){atom, str_copy});

    NK_PROF_FUNC_END();
}
