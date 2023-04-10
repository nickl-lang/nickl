#include "nk/vm/value.h"

#include <cassert>
#include <cstdalign>
#include <cstdint>
#include <limits>

#include "ir_impl.hpp"
#include "native_fn_adapter.h"
#include "nk/common/allocator.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"

static nk_typeid_t s_next_id = 1;

NkType nkt_get_array(nktype_t elem_type, size_t elem_count) {
    return {
        .as{.arr{
            .elem_type = elem_type,
            .elem_count = elem_count,
        }},
        .size = elem_type->size * elem_count,
        .align = elem_type->align,
        .tclass = NkType_Array,
        .id = s_next_id++,
    };
}

NkType nkt_get_fn(NktFnInfo info) {
    return NkType{
        .as{.fn{
            .ret_t = info.ret_t,
            .args_t = info.args_t,
            .call_conv = info.call_conv,
            .is_variadic = info.is_variadic,
        }},
        .size = sizeof(void *),
        .align = sizeof(void *),
        .tclass = NkType_Fn,
        .id = s_next_id++,
    };
}

NkType nkt_get_numeric(NkNumericValueType value_type) {
    return NkType{
        .as{.num{
            .value_type = value_type,
        }},
        .size = (size_t)NUM_TYPE_SIZE(value_type),
        .align = (uint8_t)NUM_TYPE_SIZE(value_type),
        .tclass = NkType_Numeric,
        .id = s_next_id++,
    };
}

NkType nkt_get_ptr(nktype_t target_type) {
    return NkType{
        .as{.ptr{
            .target_type = target_type,
        }},
        .size = sizeof(void *),
        .align = alignof(void *),
        .tclass = NkType_Ptr,
        .id = s_next_id++,
    };
}

NkType nkt_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride) {
    auto layout = nk_calcTupleLayout(types, count, alloc, stride);
    return NkType{
        .as{.tuple{
            .elems = layout.info_ar,
        }},
        .size = layout.size,
        .align = (uint8_t)layout.align,
        .tclass = NkType_Tuple,
        .id = s_next_id++,
    };
}

NkType nkt_get_void() {
    return NkType{
        .as{},
        .size = 0,
        .align = 1,
        .tclass = NkType_Tuple,
        .id = s_next_id++,
    };
}

void nkt_inspect(nktype_t type, NkStringBuilder sb) {
    switch (type->tclass) {
    case NkType_Array:
        nksb_printf(sb, "[%llu]", type->as.arr.elem_count);
        nkt_inspect(type->as.arr.elem_type, sb);
        break;
    case NkType_Fn: {
        nksb_printf(sb, "(");
        nktype_t const params = type->as.fn.args_t;
        for (size_t i = 0; i < params->as.tuple.elems.size; i++) {
            if (i) {
                nksb_printf(sb, ", ");
            }
            nkt_inspect(params->as.tuple.elems.data[i].type, sb);
        }
        nksb_printf(sb, ")->");
        nkt_inspect(type->as.fn.ret_t, sb);
        break;
    }
    case NkType_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
        case Int16:
        case Int32:
        case Int64:
            nksb_printf(sb, "i");
            break;
        case Uint8:
        case Uint16:
        case Uint32:
        case Uint64:
            nksb_printf(sb, "u");
            break;
        case Float32:
        case Float64:
            nksb_printf(sb, "f");
            break;
        default:
            assert(!"unreachable");
            break;
        }
        nksb_printf(sb, "%llu", (size_t)NUM_TYPE_SIZE(type->as.num.value_type) * 8);
        break;
    case NkType_Ptr:
        nksb_printf(sb, "*");
        nkt_inspect(type->as.ptr.target_type, sb);
        break;
    case NkType_Tuple: {
        nksb_printf(sb, "(");
        for (size_t i = 0; i < type->as.tuple.elems.size; i++) {
            if (i) {
                nksb_printf(sb, " ");
            }
            nkt_inspect(type->as.tuple.elems.data[i].type, sb);
            nksb_printf(sb, ",");
        }
        nksb_printf(sb, ")");
        break;
    }
    default:
        nksb_printf(sb, "type{%p}", type);
        break;
    }
}

void nkval_inspect(nkval_t val, NkStringBuilder sb) {
    switch (nkval_typeclassid(val)) {
    case NkType_Array:
        nksb_printf(sb, "[");
        for (size_t i = 0; i < nkval_typeof(val)->as.arr.elem_count; i++) {
            if (i) {
                nksb_printf(sb, " ");
            }
            nkval_inspect(nkval_array_at(val, i), sb);
            nksb_printf(sb, ",");
        }
        nksb_printf(sb, "]");
        break;
    case NkType_Fn:
        switch (nkval_typeof(val)->as.fn.call_conv) {
        case NkCallConv_Nk:
            nksb_printf(sb, "%s", nkval_as(NkIrFunct, val)->name.c_str());
            break;
        case NkCallConv_Cdecl:
            nksb_printf(sb, "%p", nkval_as(void *, val));
            break;
        }
        break;
    case NkType_Numeric:
        switch (nkval_typeof(val)->as.num.value_type) {
        case Int8:
            nksb_printf(sb, "%hhi", nkval_as(int8_t, val));
            break;
        case Uint8:
            nksb_printf(sb, "%hhu", nkval_as(uint8_t, val));
            break;
        case Int16:
            nksb_printf(sb, "%hi", nkval_as(int16_t, val));
            break;
        case Uint16:
            nksb_printf(sb, "%hu", nkval_as(uint16_t, val));
            break;
        case Int32:
            nksb_printf(sb, "%i", nkval_as(int32_t, val));
            break;
        case Uint32:
            nksb_printf(sb, "%u", nkval_as(uint32_t, val));
            break;
        case Int64:
            nksb_printf(sb, "%lli", nkval_as(int64_t, val));
            break;
        case Uint64:
            nksb_printf(sb, "%llu", nkval_as(uint64_t, val));
            break;
        case Float32:
            nksb_printf(sb, "%.*g", std::numeric_limits<float>::max_digits10, nkval_as(float, val));
            break;
        case Float64:
            nksb_printf(
                sb, "%.*g", std::numeric_limits<double>::max_digits10, nkval_as(double, val));
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case NkType_Ptr: {
        nktype_t target_type = nkval_typeof(val)->as.ptr.target_type;
        if (target_type->tclass == NkType_Array) {
            nktype_t elem_type = target_type->as.arr.elem_type;
            size_t elem_count = target_type->as.arr.elem_count;
            if (elem_type->tclass == NkType_Numeric) {
                if (elem_type->as.num.value_type == Int8 || elem_type->as.num.value_type == Uint8) {
                    nksb_printf(sb, "\"");
                    nksb_str_escape(sb, {nkval_as(char const *, val), elem_count});
                    nksb_printf(sb, "\"");
                    break;
                }
            }
        }
        nksb_printf(sb, "%p", nkval_as(void *, val));
        break;
    }
    case NkType_Tuple:
        nksb_printf(sb, "(");
        for (size_t i = 0; i < nkval_tuple_size(val); i++) {
            if (i) {
                nksb_printf(sb, " ");
            }
            nkval_inspect(nkval_tuple_at(val, i), sb);
            nksb_printf(sb, ",");
        }
        nksb_printf(sb, ")");
        break;
    default: {
        nksb_printf(sb, "value{data=%p, type=", nkval_data(val));
        nkt_inspect(nkval_typeof(val), sb);
        nksb_printf(sb, "}");
        break;
    }
    }
}

void nkval_fn_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
    switch (nkval_typeof(fn)->as.fn.call_conv) {
    case NkCallConv_Nk:
        nkir_invoke(fn, ret, args);
        break;
    case NkCallConv_Cdecl:
        nk_native_invoke(fn, ret, args);
        break;
    }
}

size_t nkval_array_size(nkval_t self) {
    return nkval_typeof(self)->as.arr.elem_count;
}

nkval_t nkval_array_at(nkval_t self, size_t i) {
    assert(i < nkval_array_size(self) && "array index out of range");
    auto const type = nkval_typeof(self);
    return {
        ((uint8_t *)nkval_data(self)) + type->as.arr.elem_type->size * i,
        type->as.arr.elem_type,
    };
}

size_t nkval_tuple_size(nkval_t self) {
    return nkval_typeof(self)->as.tuple.elems.size;
}

nkval_t nkval_tuple_at(nkval_t self, size_t i) {
    assert(i < nkval_tuple_size(self) && "tuple index out of range");
    auto const type = nkval_typeof(self);
    return {
        ((uint8_t *)nkval_data(self)) + type->as.tuple.elems.data[i].offset,
        type->as.tuple.elems.data[i].type,
    };
}

NkTupleLayout nk_calcTupleLayout(
    nktype_t const *types,
    size_t count,
    NkAllocator alloc,
    size_t stride) {
    size_t alignment = 0;
    size_t offset = 0;

    NkTupleElemInfo *info_ar =
        (NkTupleElemInfo *)nk_allocate(alloc, sizeof(NkTupleElemInfo) * count);

    for (size_t i = 0; i < count; i++) {
        nktype_t const type = types[i * stride];

        alignment = maxu(alignment, type->align);

        offset = roundUpSafe(offset, type->align);
        info_ar[i] = NkTupleElemInfo{type, offset};
        offset += type->size;
    }

    return NkTupleLayout{{info_ar, count}, roundUpSafe(offset, alignment), alignment};
}
