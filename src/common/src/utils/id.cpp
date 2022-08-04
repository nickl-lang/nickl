#include "nk/utils/id.hpp"

#include <cstring>

#include "nk/ds/arena.hpp"
#include "nk/ds/hashmap.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

namespace nk {

namespace {

LOG_USE_SCOPE(core);

Arena<char> s_arena;
HashMap<string, Id> s_str2id;
HashMap<Id, string> s_id2str;
Id s_next_id;

} // namespace

void id_init() {
    LOG_TRC(__func__);

    s_arena.reserve(1024);
    s_str2id.reserve(1024);
    s_id2str.reserve(1024);

    s_next_id = 1000;
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
        defineId(id, str);
        return id;
    }

    return *pid;
}

void defineId(Id id, string str) {
    auto str_copy = str.copy(s_arena.push(str.size));

    s_str2id.insert(str_copy, id);
    s_id2str.insert(id, str_copy);
}

} // namespace nk
