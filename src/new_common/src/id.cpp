#include "nk/common/id.h"

#include <cassert>
#include <map>
#include <string>
#include <unordered_map>

#include "nk/common/string.hpp"

namespace {

std::map<std::string, Id> s_str2id;
std::unordered_map<Id, std::string> s_id2str;
Id s_next_id = 1000;

} // namespace

string nkid2s(Id id) {
    auto it = s_id2str.find(id);
    return it != std::end(s_id2str) ? string{it->second.c_str(), it->second.size()} : string{};
}

char const *cnkid2s(Id id) {
    return nkid2s(id).data;
}

Id s2nkid(string str) {
    auto it = s_str2id.find(std_str(str));

    if (it == std::end(s_str2id)) {
        Id id = s_next_id++;
        nkid_define(id, str);
        return id;
    } else {
        return it->second;
    }
}

Id cs2nkid(char const *str) {
    return s2nkid(cs2s(str));
}

void nkid_define(Id id, string str) {
    // TODO Assert insertion of ids
    s_str2id.emplace(std_str(str), id);
    s_id2str.emplace(id, std_str(str));
}
