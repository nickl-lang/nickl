#include "nkb/types.h"

#include <float.h>

#include "ntk/common.h"
#include "ntk/string.h"
#include "ntk/utils.h"

void nkir_inspectType(NkStream out, NkIrType type) {
    if (!type) {
        nk_print(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            if (type->aggr.size) {
                nk_print(out, "{");
                NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                    if (NK_INDEX(elem, type->aggr)) {
                        nk_print(out, ", ");
                    }
                    if (elem->count > 1) {
                        nk_printf(out, "[%" PRIu32 "]", elem->count);
                    }
                    nkir_inspectType(out, elem->type);
                }
                nk_print(out, "}");
            } else {
                nk_print(out, "void");
            }
            break;

        case NkIrType_Numeric:
            switch (type->num) {
#define X(TYPE, VALUE_TYPE)    \
    case VALUE_TYPE:           \
        nk_printf(out, #TYPE); \
        break;
                NKIR_NUMERIC_ITERATE(X)
#undef X
            }
            break;

        case NkIrType_Pointer:
            nk_print(out, "ptr");
            break;
    }

    // TODO: Print alignment conservatively
}

void nkir_inspectVal(NkStream out, void *data, NkIrType type) {
    if (!data) {
        nk_print(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            nk_print(out, "{");
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                if (NK_INDEX(elem, type->aggr)) {
                    nk_print(out, ", ");
                }
                u8 *ptr = (u8 *)data + elem->offset;
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1) {
                    nk_print(out, "\"");
                    nks_escape(out, (NkString){(char const *)ptr, elem->count});
                    nk_print(out, "\"");
                } else {
                    if (elem->count > 1) {
                        nk_print(out, "[");
                    }
                    for (usize i = 0; i < elem->count; i++) {
                        if (i) {
                            nk_print(out, ", ");
                        }
                        nkir_inspectVal(out, ptr, elem->type);
                        ptr += elem->type->size;
                    }
                    if (elem->count > 1) {
                        nk_print(out, "]");
                    }
                }
            }
            nk_print(out, "}");
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
                    printFloat32Exact(out, *(f32 *)data);
                    break;
                case Float64:
                    printFloat64Exact(out, *(f64 *)data);
                    break;
            }
            break;

        case NkIrType_Pointer:
            switch (type->size) {
                case 4:
                    nk_printf(out, "0x%" PRIx32, *(i32 *)data);
                    break;

                case 8:
                    nk_printf(out, "0x%" PRIx64, *(i64 *)data);
                    break;

                default:
                    nk_assert(!"pointer must be 4 or 8 bytes");
                    break;
            }
            break;
    }
}
