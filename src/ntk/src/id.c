#include "ntk/id.h"

#include "ntk/hash_tree.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

typedef struct {
    nks key;
    nkid val;
} s2id_KV;

typedef struct {
    nkid key;
    nks val;
} id2s_KV;

static nks const *s2id_KV_GetKey(s2id_KV const *item) {
    return &item->key;
}

static nkid const *id2s_KV_GetKey(id2s_KV const *item) {
    return &item->key;
}

static hash_t nkid_hash(nkid const key) {
    return hash_val(&key);
}

static bool nkid_equal(nkid const lhs, nkid const rhs) {
    return lhs == rhs;
}

nkht_define(str2id, s2id_KV, nks, s2id_KV_GetKey, nks_hash, nks_equal);
nkht_define(id2str, id2s_KV, nkid, id2s_KV_GetKey, nkid_hash, nkid_equal);

static NkArena g_arena;
static str2id g_str2id;
static id2str g_id2str;
static nkid g_next_id = 1000;

nks nkid2s(nkid id) {
    ProfBeginFunc();
    id2s_KV const *found = id2str_find(&g_id2str, id);
    ProfEndBlock();
    return found ? found->val : (nks){0};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nks str) {
    ProfBeginFunc();

    s2id_KV const *found = str2id_find(&g_str2id, str);

    if (found) {
        ProfEndBlock();
        return found->val;
    } else {
        nkid id = g_next_id++;
        nkid_define(id, str);
        ProfEndBlock();
        return id;
    }
}

nkid cs2nkid(char const *str) {
    return s2nkid(nk_cs2s(str));
}

void nkid_define(nkid id, nks str) {
    ProfBeginFunc();

    nks const str_copy = nk_strcpy_nt(nk_arena_getAllocator(&g_arena), str);
    str2id_insert(&g_str2id, (s2id_KV){str_copy, id});
    id2str_insert(&g_id2str, (id2s_KV){id, str_copy});

    ProfEndBlock();
}
