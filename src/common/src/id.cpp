#include "nk/common/id.h"

#include "nk/common/hash_map.hpp"
#include "nk/common/string.hpp"

namespace {

struct Context {
    NkArena string_arena{};
    NkHashMap<nkstr, nkid> str2id{};
    NkHashMap<nkid, nkstr> id2str{};
    nkid next_id = 1000;

    ~Context() {
        str2id.deinit();
        id2str.deinit();

        nk_arena_free(&string_arena);
    }
} ctx;

} // namespace

nkstr nkid2s(nkid id) {
    auto found = ctx.id2str.find(id);
    return found ? *found : nkstr{};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nkstr str) {
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
    return s2nkid(nk_mkstr(str));
}

void nkid_define(nkid id, nkstr str) {
    auto const str_copy = nk_strcpy(nk_arena_getAllocator(&ctx.string_arena), str);
    ctx.str2id.insert(str_copy, id);
    ctx.id2str.insert(id, str_copy);
}
