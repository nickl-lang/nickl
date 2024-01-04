#include "ntk/id.h"

#include "ntk/hash_tree.h"
#include "ntk/string.h"

namespace {

struct s2id {
    nks key;
    nkid val;
};

struct id2s {
    nkid key;
    nks val;
};

struct Context {
    NkArena arena{};
    nkht_type(s2id) str2id{};
    nkht_type(id2s) id2str{};
    nkid next_id = 1000;

    ~Context() {
        nk_arena_free(&arena);
    }
} ctx;

} // namespace

nks nkid2s(nkid id) {
    auto found = nkht_find_val(&ctx.id2str, id);
    return found ? found->val : nks{};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nks str) {
    auto found = nkht_find_str(&ctx.str2id, str);

    if (found) {
        return found->val;
    } else {
        nkid id = ctx.next_id++;
        nkid_define(id, str);
        return id;
    }
}

nkid cs2nkid(char const *str) {
    return s2nkid(nk_cs2s(str));
}

void nkid_define(nkid id, nks str) {
    auto const str_copy = nk_strcpy_nt(nk_arena_getAllocator(&ctx.arena), str);
    nkht_insert_str(&ctx.str2id, s2id{str_copy, id});
    nkht_insert_val(&ctx.id2str, id2s{id, str_copy});
}
