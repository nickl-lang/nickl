#ifndef HEADER_GUARD_NK_COMMON_ID
#define HEADER_GUARD_NK_COMMON_ID

#include <cstddef>
#include <cstdint>

#include "nk/common/string.hpp"

using Id = uint64_t;

void id_init();
void id_deinit();

string id_to_str(Id id);

Id cstr_to_id(char const *str);
Id str_to_id(string str);

enum : Id {
    id_empty = 0,
};

#endif // HEADER_GUARD_NK_COMMON_ID
