#include "nk/common/id.hpp"

#include <cstring>

#include "nk/common/arena.hpp"
#include "nk/common/hashmap.hpp"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

namespace {

LOG_USE_SCOPE(core);

Arena<char> s_arena;
HashMap<string, Id> s_str2id;
HashMap<Id, string> s_id2str;
Id s_next_id;

void _defineId(Id id, string str) {
    auto str_copy = s_arena.push(str.size);
    str.copy(str_copy);

    s_str2id.insert(str_copy, id);
    s_id2str.insert(id, str_copy);
}

} // namespace

void id_init() {
    LOG_TRC(__func__);

    s_arena.reserve(1024);
    s_str2id.reserve(1024);
    s_id2str.reserve(1024);

    s_next_id = 1;
}

void id_deinit() {
    LOG_TRC(__func__);

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
