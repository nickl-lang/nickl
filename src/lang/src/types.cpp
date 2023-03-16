#include "nkl/lang/types.h"

#include <deque>

static std::deque<NkType> s_types;

nktype_t nkl_get_array(nktype_t elem_type, size_t elem_count) {
    return &s_types.emplace_back(nkt_get_array(elem_type, elem_count));
}

nktype_t nkl_get_fn(NktFnInfo info) {
    return &s_types.emplace_back(nkt_get_fn(info));
}

nktype_t nkl_get_numeric(NkNumericValueType value_type) {
    return &s_types.emplace_back(nkt_get_numeric(value_type));
}

nktype_t nkl_get_ptr(nktype_t target_type) {
    return &s_types.emplace_back(nkt_get_ptr(target_type));
}

nktype_t nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride) {
    return &s_types.emplace_back(nkt_get_tuple(alloc, types, count, stride));
}

nktype_t nkl_get_void() {
    return &s_types.emplace_back(nkt_get_void());
}
