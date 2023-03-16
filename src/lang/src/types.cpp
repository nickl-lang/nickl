#include "nkl/lang/types.h"

#include <new>

nktype_t nkl_get_array(NkAllocator alloc, nktype_t elem_type, size_t elem_count) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{nkt_get_array(elem_type, elem_count)};
}

nktype_t nkl_get_fn(NkAllocator alloc, NktFnInfo info) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{nkt_get_fn(info)};
}

nktype_t nkl_get_numeric(NkAllocator alloc, NkNumericValueType value_type) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{nkt_get_numeric(value_type)};
}

nktype_t nkl_get_ptr(NkAllocator alloc, nktype_t target_type) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{nkt_get_ptr(target_type)};
}

nktype_t nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride) {
    return new (nk_allocate(alloc, sizeof(NkType)))
        NkType{nkt_get_tuple(alloc, types, count, stride)};
}

nktype_t nkl_get_void(NkAllocator alloc) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{nkt_get_void()};
}
