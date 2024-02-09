#include "nkb/common.h"

#include <cassert>
#include <cctype>
#include <limits>

#include "ntk/allocator.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/utils.h"

void nkirt_inspect(nktype_t type, nk_stream out) {
    if (!type) {
        nk_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
    case NkType_Aggregate:
        if (type->as.aggr.elems.size) {
            nk_printf(out, "{");
            for (usize i = 0; i < type->as.aggr.elems.size; i++) {
                if (i) {
                    nk_printf(out, ", ");
                }
                auto const &elem = type->as.aggr.elems.data[i];
                if (elem.count > 0) {
                    nk_printf(out, "[%" PRIu64 "]", elem.count);
                }
                nkirt_inspect(elem.type, out);
            }
            nk_printf(out, "}");
        } else {
            nk_printf(out, "void");
        }
        break;
    case NkType_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
            nk_printf(out, "i8");
            break;
        case Uint8:
            nk_printf(out, "u8");
            break;
        case Int16:
            nk_printf(out, "i16");
            break;
        case Uint16:
            nk_printf(out, "u16");
            break;
        case Int32:
            nk_printf(out, "i32");
            break;
        case Uint32:
            nk_printf(out, "u32");
            break;
        case Int64:
            nk_printf(out, "i64");
            break;
        case Uint64:
            nk_printf(out, "u64");
            break;
        case Float32:
            nk_printf(out, "f32");
            break;
        case Float64:
            nk_printf(out, "f64");
            break;
        }
        break;
    case NkType_Pointer:
        nk_printf(out, "*");
        nkirt_inspect(type->as.ptr.target_type, out);
        break;
    case NkType_Procedure:
        nk_printf(out, "(");
        for (usize i = 0; i < type->as.proc.info.args_t.size; i++) {
            if (i) {
                nk_printf(out, ", ");
            }
            nkirt_inspect(type->as.proc.info.args_t.data[i], out);
        }
        if (type->as.proc.info.flags & NkProcVariadic) {
            nk_printf(out, ", ...");
        }
        nk_printf(out, ")->");
        nkirt_inspect(type->as.proc.info.ret_t, out);
        break;
    default:
        assert(!"unreachable");
    }
}

void nkirv_inspect(void *data, nktype_t type, nk_stream out) {
    if (!data) {
        nk_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
    case NkType_Aggregate:
        for (usize elemi = 0; elemi < type->as.aggr.elems.size; elemi++) {
            auto const &elem = type->as.aggr.elems.data[elemi];
            auto ptr = (u8 *)data + elem.offset;
            if (elem.type->kind == NkType_Numeric && elem.type->size == 1) {
                nk_printf(out, "\"");
                nks_escape(out, {(char const *)ptr, elem.count});
                nk_printf(out, "\"");
            } else {
                if (elemi == 0) {
                    nk_printf(out, "{");
                }
                if (elem.count) {
                    nk_printf(out, "[");
                }
                for (usize i = 0; i < elem.count; i++) {
                    if (i) {
                        nk_printf(out, ", ");
                    }
                    nkirv_inspect(ptr, elem.type, out);
                    ptr += elem.type->size;
                }
                if (elem.count) {
                    nk_printf(out, "]");
                }
                if (elemi == type->as.aggr.elems.size - 1) {
                    nk_printf(out, "}");
                }
            }
        }
        break;
    case NkType_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
            nk_printf(out, "%" PRIi8, *(i8 *)data);
            break;
        case Uint8:
            nk_printf(out, "%" PRIu8, *(u8 *)data);
            break;
        case Int16:
            nk_printf(out, "%" PRIi16, *(i16 *)data);
            break;
        case Uint16:
            nk_printf(out, "%" PRIu16, *(u16 *)data);
            break;
        case Int32:
            nk_printf(out, "%" PRIi32, *(i32 *)data);
            break;
        case Uint32:
            nk_printf(out, "%" PRIu32, *(u32 *)data);
            break;
        case Int64:
            nk_printf(out, "%" PRIi64, *(i64 *)data);
            break;
        case Uint64:
            nk_printf(out, "%" PRIu64, *(u64 *)data);
            break;
        case Float32:
            nk_printf(out, "%.*g", std::numeric_limits<f32>::max_digits10, *(f32 *)data);
            break;
        case Float64:
            nk_printf(out, "%.*g", std::numeric_limits<f64>::max_digits10, *(f64 *)data);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case NkType_Pointer:
    case NkType_Procedure:
        nk_printf(out, "%p", *(void **)data);
        break;
    default:
        assert(!"unreachable");
    }
}

NkIrAggregateLayout nkir_calcAggregateLayout(
    NkAllocator alloc,
    nktype_t const *elem_types,
    usize const *elem_counts,
    usize n,
    usize type_stride,
    usize count_stride) {
    ProfFunc();
    usize alignment = 0;
    usize offset = 0;

    auto info_ar = nk_alloc_t<NkIrAggregateElemInfo>(alloc, n);

    for (usize i = 0; i < n; i++) {
        auto const type = elem_types[i * type_stride];
        auto const elem_count = elem_counts[i * count_stride];

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
