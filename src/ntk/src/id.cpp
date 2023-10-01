#include "ntk/id.h"

#include "ntk/hash_map.hpp"
#include "ntk/string.hpp"

namespace {

struct Context {
    NkArena string_arena{};
    NkHashMap<nks, nkid> str2id{};
    NkHashMap<nkid, nks> id2str{};
    nkid next_id = 1000;

    ~Context() {
        str2id.deinit();
        id2str.deinit();

        nk_arena_free(&string_arena);
    }
} ctx;

} // namespace

nks nkid2s(nkid id) {
    auto found = ctx.id2str.find(id);
    return found ? *found : nks{};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nks str) {
    auto found = ctx.str2id.find(str);

    if (found) {
        return *found;
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
    auto const str_copy = nk_strcpy(nk_arena_getAllocator(&ctx.string_arena), str);
    ctx.str2id.insert(str_copy, id);
    ctx.id2str.insert(id, str_copy);
}
