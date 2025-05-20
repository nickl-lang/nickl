#include "nkb/types.h"

#include <float.h>

#include "ntk/string.h"

void nkir_inspectType(NkIrType type, NkStream out) {
    if (!type) {
        nk_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Union:
        case NkIrType_Aggregate:
            if (type->aggr.size) {
                nk_printf(out, "{");
                for (usize i = 0; i < type->aggr.size; i++) {
                    if (i) {
                        nk_printf(out, type->kind == NkIrType_Union ? " | " : ", ");
                    }
                    NkIrAggregateElemInfo const *elem = &type->aggr.data[i];
                    if (elem->count > 1) {
                        nk_printf(out, "[%" PRIu32 "]", elem->count);
                    }
                    nkir_inspectType(elem->type, out);
                }
                nk_printf(out, "}");
            } else {
                nk_printf(out, "void");
            }
            break;

        case NkIrType_Numeric:
            switch (type->num) {
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
    }

    // TODO: Print alignment conservatively
}

void nkir_inspectVal(void *data, NkIrType type, NkStream out) {
    if (!data) {
        nk_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Union: // TODO: Remember and print only active element of a union
        case NkIrType_Aggregate:
            for (usize elemi = 0; elemi < type->aggr.size; elemi++) {
                if (elemi) {
                    nk_printf(out, type->kind == NkIrType_Union ? " | " : ", ");
                }
                NkIrAggregateElemInfo const *elem = &type->aggr.data[elemi];
                u8 *ptr = (u8 *)data + elem->offset;
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1) {
                    nk_printf(out, "\"");
                    nks_escape(out, (NkString){(char const *)ptr, elem->count});
                    nk_printf(out, "\"");
                } else {
                    if (elemi == 0) {
                        nk_printf(out, "{");
                    }
                    if (elem->count > 1) {
                        nk_printf(out, "[");
                    }
                    for (usize i = 0; i < elem->count; i++) {
                        if (i) {
                            nk_printf(out, ", ");
                        }
                        nkir_inspectVal(ptr, elem->type, out);
                        ptr += elem->type->size;
                    }
                    if (elem->count > 1) {
                        nk_printf(out, "]");
                    }
                    if (elemi == type->aggr.size - 1) {
                        nk_printf(out, "}");
                    }
                }
            }
            break;

        case NkIrType_Numeric:
            switch (type->num) {
#define X(TYPE, VALUE_TYPE)                                   \
    case VALUE_TYPE:                                          \
        nk_printf(out, "%" NK_CAT(PRI, TYPE), *(TYPE *)data); \
        break;
                NKIR_NUMERIC_ITERATE_INT(X)
#undef X
                case Float32:
                    nk_printf(out, "%.*g", FLT_DIG, *(f32 *)data);
                    break;
                case Float64:
                    nk_printf(out, "%.*g", DBL_DIG, *(f64 *)data);
                    break;
            }
            break;
    }
}
