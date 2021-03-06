#include "nk/common/id.hpp"

#include <cstring>

#include "nk/common/arena.hpp"
#include "nk/common/hashmap.hpp"
#include "nk/common/utils.hpp"

namespace {

LOG_USE_SCOPE(core);

struct StrHashMapContext {
    static hash_t hash(string key) {
        return hash_array((uint8_t const *)key.data, (uint8_t const *)key.data + key.size);
    }

    static bool equal_to(string lhs, string rhs) {
        return lhs.size == rhs.size && std::strncmp(lhs.data, rhs.data, lhs.size) == 0;
    }
};

ArenaAllocator s_arena;
HashMap<string, Id, StrHashMapContext> s_str2id;
HashMap<Id, string> s_id2str;
Id s_next_id;

void _defineId(Id id, string str) {
    char *mem = (char *)s_arena.alloc(str.size);
    std::memcpy(mem, str.data, str.size);

    string str_copy{mem, str.size};
    s_str2id.insert(str_copy) = id;
    s_id2str.insert(id) = str_copy;
}

} // namespace

void id_init() {
    LOG_TRC("id_init");

    s_arena.init(1024);
    s_str2id.init(1024);
    s_id2str.init(1024);

    s_next_id = 1;
}

void id_deinit() {
    LOG_TRC("id_deinit");

    s_id2str.deinit();
    s_str2id.deinit();
    s_arena.deinit();
}

string id2s(Id id) {
    string *pstr = s_id2str.find(id);
    return pstr ? *pstr : string{};
}

Id cs2id(char const *str) {
    return s2id(string{str, std::strlen(str)});
}

Id s2id(string str) {
    Id *pid = s_str2id.find(str);

    if (nullptr == pid) {
        Id id = s_next_id++;
        _defineId(id, str);
        return id;
    }

    return *pid;
}
