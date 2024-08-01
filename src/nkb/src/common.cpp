#include "nkb/common.h"

#include <limits>

#include "ntk/allocator.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/utils.h"

void nkirt_inspect(nktype_t type, NkStream out) {
    if (!type) {
        nk_stream_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            if (type->as.aggr.elems.size) {
                nk_stream_printf(out, "{");
                for (usize i = 0; i < type->as.aggr.elems.size; i++) {
                    if (i) {
                        nk_stream_printf(out, ", ");
                    }
                    auto const &elem = type->as.aggr.elems.data[i];
                    if (elem.count > 1) {
                        nk_stream_printf(out, "[%" PRIu64 "]", elem.count);
                    }
                    nkirt_inspect(elem.type, out);
                }
                nk_stream_printf(out, "}");
            } else {
                nk_stream_printf(out, "void");
            }
            break;
        case NkIrType_Numeric:
            switch (type->as.num.value_type) {
                case Int8:
                    nk_stream_printf(out, "i8");
                    break;
                case Uint8:
                    nk_stream_printf(out, "u8");
                    break;
                case Int16:
                    nk_stream_printf(out, "i16");
                    break;
                case Uint16:
                    nk_stream_printf(out, "u16");
                    break;
                case Int32:
                    nk_stream_printf(out, "i32");
                    break;
                case Uint32:
                    nk_stream_printf(out, "u32");
                    break;
                case Int64:
                    nk_stream_printf(out, "i64");
                    break;
                case Uint64:
                    nk_stream_printf(out, "u64");
                    break;
                case Float32:
                    nk_stream_printf(out, "f32");
                    break;
                case Float64:
                    nk_stream_printf(out, "f64");
                    break;
            }
            break;
        case NkIrType_Pointer:
            nk_stream_printf(out, "ptr");
            break;
        case NkIrType_Procedure:
            nk_stream_printf(out, "(");
            for (usize i = 0; i < type->as.proc.info.args_t.size; i++) {
                if (i) {
                    nk_stream_printf(out, ", ");
                }
                nkirt_inspect(type->as.proc.info.args_t.data[i], out);
            }
            if (type->as.proc.info.flags & NkProcVariadic) {
                nk_stream_printf(out, ", ...");
            }
            nk_stream_printf(out, ") ");
            nkirt_inspect(type->as.proc.info.ret_t, out);
            break;
        default:
            nk_assert(!"unreachable");
    }
}

void nkirv_inspect(void *data, nktype_t type, NkStream out) {
    if (!data) {
        nk_stream_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            for (usize elemi = 0; elemi < type->as.aggr.elems.size; elemi++) {
                auto const &elem = type->as.aggr.elems.data[elemi];
                auto ptr = (u8 *)data + elem.offset;
                if (elem.type->kind == NkIrType_Numeric && elem.type->size == 1) {
                    nk_stream_printf(out, "\"");
                    nks_escape(out, {(char const *)ptr, elem.count});
                    nk_stream_printf(out, "\"");
                } else {
                    if (elemi == 0) {
                        nk_stream_printf(out, "{");
                    }
                    if (elem.count) {
                        nk_stream_printf(out, "[");
                    }
                    for (usize i = 0; i < elem.count; i++) {
                        if (i) {
                            nk_stream_printf(out, ", ");
                        }
                        nkirv_inspect(ptr, elem.type, out);
                        ptr += elem.type->size;
                    }
                    if (elem.count) {
                        nk_stream_printf(out, "]");
                    }
                    if (elemi == type->as.aggr.elems.size - 1) {
                        nk_stream_printf(out, "}");
                    }
                }
            }
            break;
        case NkIrType_Numeric:
            switch (type->as.num.value_type) {
                case Int8:
                    nk_stream_printf(out, "%" PRIi8, *(i8 *)data);
                    break;
                case Uint8:
                    nk_stream_printf(out, "%" PRIu8, *(u8 *)data);
                    break;
                case Int16:
                    nk_stream_printf(out, "%" PRIi16, *(i16 *)data);
                    break;
                case Uint16:
                    nk_stream_printf(out, "%" PRIu16, *(u16 *)data);
                    break;
                case Int32:
                    nk_stream_printf(out, "%" PRIi32, *(i32 *)data);
                    break;
                case Uint32:
                    nk_stream_printf(out, "%" PRIu32, *(u32 *)data);
                    break;
                case Int64:
                    nk_stream_printf(out, "%" PRIi64, *(i64 *)data);
                    break;
                case Uint64:
                    nk_stream_printf(out, "%" PRIu64, *(u64 *)data);
                    break;
                case Float32:
                    nk_stream_printf(out, "%.*g", std::numeric_limits<f32>::max_digits10, *(f32 *)data);
                    break;
                case Float64:
                    nk_stream_printf(out, "%.*g", std::numeric_limits<f64>::max_digits10, *(f64 *)data);
                    break;
                default:
                    nk_assert(!"unreachable");
                    break;
            }
            break;
        case NkIrType_Pointer:
        case NkIrType_Procedure:
            nk_stream_printf(out, "%p", *(void **)data);
            break;
        default:
            nk_assert(!"unreachable");
    }
}

NkIrAggregateLayout nkir_calcAggregateLayout(
    NkAllocator alloc,
    nktype_t const *elem_types,
    usize const *elem_counts,
    usize n,
    usize type_stride,
    usize count_stride) {
    NK_PROF_FUNC();
    usize alignment = 1;
    usize offset = 0;

    auto info_ar = nk_allocT<NkIrAggregateElemInfo>(alloc, n);

    auto type_it = elem_types;
    auto elem_count_it = elem_counts;

    for (usize i = 0; i < n; i++) {
        auto const type = *type_it;
        type_it = (nktype_t const *)((u8 const *)type_it + type_stride);

        auto const elem_count = elem_counts ? *elem_count_it : 1;
        elem_count_it = (usize const *)((u8 const *)elem_count_it + count_stride);

        alignment = nk_maxu(alignment, type->align);

        offset = nk_roundUpSafe(offset, type->align);
        info_ar[i] = {
            type,
            elem_count,
            offset,
        };
        offset += type->size * elem_count;
    }

    return NkIrAggregateLayout{{info_ar, n}, nk_roundUpSafe(offset, alignment), alignment};
}
