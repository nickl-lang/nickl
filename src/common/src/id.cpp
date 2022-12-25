#include "nk/common/id.h"

#include <cassert>
#include <map>
#include <string>
#include <unordered_map>

#include "nk/common/string.hpp"

namespace {

std::map<std::string, nkid> s_str2id;
std::unordered_map<nkid, std::string> s_id2str;
nkid s_next_id = 1000;

} // namespace

nkstr nkid2s(nkid id) {
    auto it = s_id2str.find(id);
    return it != std::end(s_id2str) ? nkstr{it->second.c_str(), it->second.size()} : nkstr{};
}

char const *nkid2cs(nkid id) {
    return nkid2s(id).data;
}

nkid s2nkid(nkstr str) {
    auto it = s_str2id.find(std_str(str));

    if (it == std::end(s_str2id)) {
        nkid id = s_next_id++;
        nkid_define(id, str);
        return id;
    } else {
        return it->second;
    }
}

nkid cs2nkid(char const *str) {
    return s2nkid(cs2s(str));
}

void nkid_define(nkid id, nkstr str) {
    // TODO Assert insertion of ids
    s_str2id.emplace(std_str(str), id);
    s_id2str.emplace(id, std_str(str));
}
