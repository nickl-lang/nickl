#include "nkb/common.h"

#include <cassert>
#include <cctype>
#include <limits>

#include "nk/common/allocator.hpp"
#include "nk/common/utils.h"

void nkirt_inspect(nktype_t type, NkStringBuilder sb) {
    if (!type) {
        nksb_printf(sb, "(null)");
        return;
    }
    switch (type->kind) {
    case NkType_Basic:
        switch (type->as.basic.value_type) {
        case Int8:
            nksb_printf(sb, "i8");
            break;
        case Uint8:
            nksb_printf(sb, "u8");
            break;
        case Int16:
            nksb_printf(sb, "i16");
            break;
        case Uint16:
            nksb_printf(sb, "u16");
            break;
        case Int32:
            nksb_printf(sb, "i32");
            break;
        case Uint32:
            nksb_printf(sb, "u32");
            break;
        case Int64:
            nksb_printf(sb, "i64");
            break;
        case Uint64:
            nksb_printf(sb, "u64");
            break;
        case Float32:
            nksb_printf(sb, "f32");
            break;
        case Float64:
            nksb_printf(sb, "f64");
            break;
        }
        break;
    case NkType_Aggregate:
        nksb_printf(sb, "{");
        for (size_t i = 0; i < type->as.aggregate.elems.size; i++) {
            if (i) {
                nksb_printf(sb, ", ");
            }
            auto const &elem = type->as.aggregate.elems.data[i];
            if (elem.count > 0) {
                nksb_printf(sb, "[%" PRIu64 "]", elem.count);
            }
            nkirt_inspect(elem.type, sb);
        }
        nksb_printf(sb, "}");
        break;
    default:
        assert(!"unreachable");
    }
}

void nkirv_inspect(void *data, nktype_t type, NkStringBuilder sb) {
    if (!data) {
        nksb_printf(sb, "(null)");
        return;
    }
    switch (type->kind) {
    case NkType_Basic:
        switch (type->as.basic.value_type) {
        case Int8:
            nksb_printf(sb, "%" PRIi8, *reinterpret_cast<int8_t *>(data));
            break;
        case Uint8:
            nksb_printf(sb, "%" PRIu8, *reinterpret_cast<uint8_t *>(data));
            break;
        case Int16:
            nksb_printf(sb, "%" PRIi16, *reinterpret_cast<int16_t *>(data));
            break;
        case Uint16:
            nksb_printf(sb, "%" PRIu16, *reinterpret_cast<uint16_t *>(data));
            break;
        case Int32:
            nksb_printf(sb, "%" PRIi32, *reinterpret_cast<int32_t *>(data));
            break;
        case Uint32:
            nksb_printf(sb, "%" PRIu32, *reinterpret_cast<uint32_t *>(data));
            break;
        case Int64:
            nksb_printf(sb, "%" PRIi64, *reinterpret_cast<int64_t *>(data));
            break;
        case Uint64:
            nksb_printf(sb, "%" PRIu64, *reinterpret_cast<uint64_t *>(data));
            break;
        case Float32:
            nksb_printf(sb, "%.*g", std::numeric_limits<float>::max_digits10, *reinterpret_cast<float *>(data));
            break;
        case Float64:
            nksb_printf(sb, "%.*g", std::numeric_limits<double>::max_digits10, *reinterpret_cast<double *>(data));
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case NkType_Aggregate:
        for (size_t elemi = 0; elemi < type->as.aggregate.elems.size; elemi++) {
            auto const &elem = type->as.aggregate.elems.data[elemi];
            auto ptr = (uint8_t *)data + elem.offset;
            if (elem.type->kind == NkType_Basic && elem.type->size == 1) {
                nksb_printf(sb, "\"");
                nksb_str_escape(sb, {(char const *)ptr, elem.count});
                nksb_printf(sb, "\"");
            } else {
                if (elem.count) {
                    nksb_printf(sb, "[");
                }
                for (size_t i = 0; i < elem.count; i++) {
                    if (i) {
                        nksb_printf(sb, ", ");
                    }
                    nkirv_inspect(ptr, elem.type, sb);
                    ptr += elem.type->size;
                }
                if (elem.count) {
                    nksb_printf(sb, "]");
                }
            }
        }
        break;
    default:
        assert(!"unreachable");
    }
}

NkIrAggregateLayout nkir_calcAggregateLayout(
    NkAllocator alloc,
    nktype_t const *elem_types,
    size_t const *elem_counts,
    size_t n,
    size_t stride) {
    size_t alignment = 0;
    size_t offset = 0;

    auto info_ar = nk_alloc_t<NkIrAggregateElemInfo>(alloc, n);

    for (size_t i = 0; i < n; i++) {
        auto const type = elem_types[i * stride];
        auto const elem_count = elem_counts[i * stride];

        alignment = maxu(alignment, type->align);

        offset = roundUpSafe(offset, type->align);
        info_ar[i] = {
            type,
            elem_count,
            offset,
        };
        offset += type->size * elem_count;
    }

    return NkIrAggregateLayout{{info_ar, n}, roundUpSafe(offset, alignment), alignment};
}