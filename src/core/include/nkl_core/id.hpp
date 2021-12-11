#ifndef HEADER_GUARD_ID
#define HEADER_GUARD_ID

#include <cstddef>
#include <cstdint>

#include "nkl_core/common.hpp"

using Id = uint64_t;

void id_init();
void id_deinit();

string id_to_str(Id id);

Id cstr_to_id(char const *str);
Id str_to_id(string str);

enum EId : Id {
    id_invalid = 0,

#define X(type, node_id) id_##node_id,
#include "nkl_core/nodes.inl"

    _id_count,
};

#endif // HEADER_GUARD_ID
