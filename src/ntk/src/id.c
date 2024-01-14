#include "ntk/id.h"

#include "ntk/hash_tree.h"
#include "ntk/string.h"

typedef struct {
    nks key;
    nkid val;
} s2id_KV;

typedef struct {
    nkid key;
    nks val;
} id2s_KV;

static NkArena g_arena;
static nkht_type(s2id_KV) g_str2id;
static nkht_type(id2s_KV) g_id2str;
static nkid g_next_id = 1000;

nks nkid2s(nkid id) {
    id2s_KV const *found = nkht_find_val(&g_id2str, id);
    return found ? found->val : (nks){0};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nks str) {
    s2id_KV const *found = nkht_find_str(&g_str2id, str);

    if (found) {
        return found->val;
    } else {
        nkid id = g_next_id++;
        nkid_define(id, str);
        return id;
    }
}

nkid cs2nkid(char const *str) {
    return s2nkid(nk_cs2s(str));
}

void nkid_define(nkid id, nks str) {
    nks const str_copy = nk_strcpy_nt(nk_arena_getAllocator(&g_arena), str);
    nkht_insert_str(&g_str2id, ((s2id_KV){str_copy, id}));
    nkht_insert_val(&g_id2str, ((id2s_KV){id, str_copy}));
}
