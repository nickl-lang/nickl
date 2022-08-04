#ifndef HEADER_GUARD_NK_COMMON_ID
#define HEADER_GUARD_NK_COMMON_ID

#include <cstddef>
#include <cstdint>

#include "nk/str/string.hpp"

namespace nk {

using Id = uint64_t;

void id_init();
void id_deinit();

string id2s(Id id);

Id cs2id(char const *str);
Id s2id(string str);

void defineId(Id id, string str);

enum : Id {
    id_empty = 0,
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_ID
