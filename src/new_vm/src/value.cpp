#include "nk/vm/value.h"

#include <cstdalign>
#include <cstdint>
#include <new>

#include "nk/common/allocator.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"

namespace {

nk_typeid_t s_next_type_id = 1;

struct TupleLayout {
    NkTupleElemInfoArray info_ar;
    size_t size;
    size_t align;
};

TupleLayout _calcTupleLayout(
    nktype_t const *types,
    size_t count,
    NkAllocator *alloc,
    size_t stride) {
    size_t alignment = 0;
    size_t offset = 0;

    NkTupleElemInfo *info_ar =
        (NkTupleElemInfo *)nk_allocate(alloc, sizeof(NkTupleElemInfo) * count);

    for (size_t i = 0; i < count; i++) {
        nktype_t const type = types[i * stride];

        alignment = maxu(alignment, type->alignment);

        offset = roundUpSafe(offset, type->alignment);
        info_ar[i] = NkTupleElemInfo{type, offset};
        offset += type->size;
    }

    return TupleLayout{{info_ar, count}, roundUpSafe(offset, alignment), alignment};
}

} // namespace

nktype_t nkt_get_array(NkAllocator *alloc, nktype_t elem_type, size_t elem_count) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{.arr{
            .elem_type = elem_type,
            .elem_count = elem_count,
        }},
        .id = s_next_type_id++,
        .size = elem_type->size * elem_count,
        .alignment = elem_type->alignment,
        .typeclass_id = NkType_Array,
    };
}

nktype_t nkt_get_fn(
    NkAllocator *alloc,
    nktype_t ret_t,
    nktype_t args_t,
    void *closure,
    NkCallConv call_conv,
    bool is_variadic) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{.fn{
            .ret_t = ret_t,
            .args_t = args_t,
            .closure = closure,
            .call_conv = call_conv,
            .is_variadic = is_variadic,
        }},
        .id = s_next_type_id++,
        .size = sizeof(void *),
        .alignment = sizeof(void *),
        .typeclass_id = NkType_Fn,
    };
}

nktype_t nkt_get_numeric(NkAllocator *alloc, NkNumericValueType value_type) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{.num{
            .value_type = value_type,
        }},
        .id = s_next_type_id++,
        .size = (size_t)NUM_TYPE_SIZE(value_type),
        .alignment = (uint8_t)NUM_TYPE_SIZE(value_type),
        .typeclass_id = NkType_Numeric,
    };
}

nktype_t nkt_get_ptr(NkAllocator *alloc, nktype_t target_type) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{.ptr{
            .target_type = target_type,
        }},
        .id = s_next_type_id++,
        .size = sizeof(void *),
        .alignment = alignof(void *),
        .typeclass_id = NkType_Ptr,
    };
}

nktype_t nkt_get_tuple(NkAllocator *alloc, nktype_t const *types, size_t count, size_t stride) {
    auto layout = _calcTupleLayout(types, count, alloc, stride);
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{.tuple{
            .elems = layout.info_ar,
        }},
        .id = s_next_type_id++,
        .size = layout.size,
        .alignment = (uint8_t)layout.align,
        .typeclass_id = NkType_Tuple,
    };
}

nktype_t nkt_get_void(NkAllocator *alloc) {
    return new (nk_allocate(alloc, sizeof(NkType))) NkType{
        .as{},
        .id = s_next_type_id++,
        .size = 0,
        .alignment = 1,
        .typeclass_id = NkType_Void,
    };
}

void nkt_inspect(nktype_t type, NkStringBuilder sb) {
}

void nkval_inspect(nkval_t val, NkStringBuilder sb) {
}

void nkval_fn_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
}
