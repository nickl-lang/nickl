#include "nkl/core/types.h"

#include <string.h>

#include "nickl_impl.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/utils.h"

// TODO: Use 128bit hash directly?
static NkHash64 NkHash128_KeyHash(NkHash128 key) {
    return *(NkHash64 *)key.bytes;
}

// NkHash128 const *NklType_T_GetKey(NklType item) {
//     return &item->key;
// }

NK_HASH_TREE_ARRAY_IMPL_KV(NklTypeMap, NkHash128, NklType, NkHash128_KeyHash, nk_hash128_equal);
// NK_HASH_TREE_ARRAY_IMPL(NklTypeMap, NklType_T, NkHash128, NklType_T_GetKey, NkHash128_KeyHash, nk_hash128_equal);

#define HASH_VAL(HASHER, T, VAL)                                     \
    do {                                                             \
        T const _val = (T)(VAL);                                     \
        nk_hash128_update(&hasher, (u8 const *)&_val, sizeof(_val)); \
    } while (0)

void nkl_types_init(NklTypeStorage *st, NkArena *arena) {
    *st = (NklTypeStorage){
        .arena = arena,
        .map = (NklTypeMap){.alloc = nk_arena_getAllocator(arena)},
        .next_tclass = NklTypeClassCount,
    };
}

NklTypeClass nkl_type_newClass(NklState nkl) {
    return nkl->types.next_tclass++;
}

NklType nkl_type_getIncomplete(NklState nkl) {
    NklTypeStorage *st = &nkl->types;
    NklType_T *type = nk_arena_allocT(st->arena, NklType_T);
    *type = (NklType_T){0};
    return type;
}

void nkl_type_complete(NklState NK_UNUSED nkl, NklType dst, NklType src) {
    nk_assert(!nkl_type_isComplete(dst));
    nk_assert(nkl_type_isComplete(src));
    *(NklType_T *)dst = *src;
}

bool nkl_type_isComplete(NklType type) {
    return type->tclass != NklType_None;
}

NklType nkl_type_getFromCache(NklState nkl, NkHash128 hash) {
    NklTypeStorage *st = &nkl->types;
    NklType *found = NklTypeMap_find(&st->map, hash);
    if (found) {
        return *found;
    } else {
        NklType const type = nkl_type_getIncomplete(nkl);
        NklTypeMap_insert(&st->map, hash, type);
        return type;
    }
}

NklType nkl_type_getTypeclassInstance(NklState nkl, NklTypeClass tclass, NklType base) {
    NklType_T const src_t = {
        ._ir_type = base->_ir_type,
        .base_t = base,
        .size = base->size,
        .align = base->align,
        .tclass = tclass,
    };
    return nkl_type_getDistinct(nkl, &src_t);
}

static void completeAggregate(NklState nkl, NklType_T *type, NklTypeStridedArray types) {
    NklTypeStorage *st = &nkl->types;

    NklTypeArray types_copy = {0};
    NKS_COPY_STRIDED(nk_arena_getAllocator(st->arena), &types_copy, types);

    NkIrAggregateElemInfoDynArray ir_elems = {.alloc = nk_arena_getAllocator(st->arena)};

    usize align = 0;
    usize offset = 0;
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        NklType const elem_t = *it;

        align = nk_maxu(align, elem_t->align);
        offset = nk_alignToPowerOf2(offset, elem_t->align);

        nkda_append(
            &ir_elems,
            ((NkIrAggregateElemInfo){
                .type = nkl_type_toIr(elem_t),
                .count = 1,
                .offset = offset,
            }));

        offset += elem_t->size;
    }
    usize const size = nk_alignToPowerOf2(offset, align);

    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .aggr = {NKS_INIT(ir_elems)},
                .size = size,
                .align = align,
                .id = 0,
                .kind = NkIrType_Aggregate,
            },
        .base_t = NULL,
        .size = size,
        .align = align,
        .tclass = NklType_Aggregate,
        .as.aggr =
            {
                .types = types_copy,
            },
    };
}

NklType nkl_type_getAggregateDistinct(NklState nkl, NklTypeStridedArray types) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeAggregate(nkl, type, types);
    return type;
}

NklType nkl_type_getAggregate(NklState nkl, NklTypeStridedArray types) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Aggregate);
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        NklType const type = *it;
        HASH_VAL(hasher, intptr_t, type);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeAggregate(nkl, type, types);
    }
    return type;
}

static void completeArray(NklState NK_UNUSED nkl, NklType_T *type, NklType elem_t, usize count) {
    NklTypeStorage *st = &nkl->types;

    NkIrAggregateElemInfo *elem = nk_arena_allocT(st->arena, NkIrAggregateElemInfo);
    *elem = (NkIrAggregateElemInfo){
        .type = nkl_type_toIr(elem_t),
        .count = count,
        .offset = 0,
    };

    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .aggr = {elem, 1},
                .size = elem_t->size * count,
                .align = elem_t->align,
                .id = 0,
                .kind = NkIrType_Aggregate,
            },
        .base_t = NULL,
        .size = elem_t->size * count,
        .align = elem_t->align,
        .tclass = NklType_Array,
        .as.arr =
            {
                .elem_t = elem_t,
                .count = count,
            },
    };
}

NklType nkl_type_getArrayDistinct(NklState nkl, NklType elem_t, usize count) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeArray(nkl, type, elem_t, count);
    return type;
}

NklType nkl_type_getArray(NklState nkl, NklType elem_t, usize count) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Array);
    HASH_VAL(hasher, intptr_t, elem_t);
    HASH_VAL(hasher, usize, count);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeArray(nkl, type, elem_t, count);
    }
    return type;
}

static void completeBool(NklState NK_UNUSED nkl, NklType_T *type) {
    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .num = Uint8,
                .size = 1,
                .align = 1,
                .id = 0,
                .kind = NkIrType_Numeric,
            },
        .base_t = NULL,
        .size = 1,
        .align = 1,
        .tclass = NklType_Bool,
    };
}

NklType nkl_type_getBoolDistinct(NklState nkl) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeBool(nkl, type);
    return type;
}

NklType nkl_type_getBool(NklState nkl) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Bool);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeBool(nkl, type);
    }
    return type;
}

static void completeNumeric(NklState NK_UNUSED nkl, NklType_T *type, NkIrNumericValueType value_type) {
    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .num = value_type,
                .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .id = 0,
                .kind = NkIrType_Numeric,
            },
        .base_t = NULL,
        .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
        .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
        .tclass = NklType_Numeric,
        .as.num =
            {
                .value_type = value_type,
            },
    };
}

NklType nkl_type_getNumericDistinct(NklState nkl, NkIrNumericValueType value_type) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeNumeric(nkl, type, value_type);
    return type;
}

NklType nkl_type_getNumeric(NklState nkl, NkIrNumericValueType value_type) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Numeric);
    HASH_VAL(hasher, NkIrNumericValueType, value_type);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeNumeric(nkl, type, value_type);
    }
    return type;
}

static void completePointer(NklState NK_UNUSED nkl, NklType_T *type, usize word_size, NklType target_t, bool is_const) {
    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .size = word_size,
                .align = word_size,
                .id = 0,
                .kind = NkIrType_Pointer,
            },
        .base_t = NULL,
        .size = word_size,
        .align = word_size,
        .tclass = NklType_Pointer,
        .as.ptr =
            {
                .target_t = target_t,
                .is_const = is_const,
            },
    };
}

NklType nkl_type_getPointerDistinct(NklState nkl, usize word_size, NklType target_t, bool is_const) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completePointer(nkl, type, word_size, target_t, is_const);
    return type;
}

NklType nkl_type_getPointer(NklState nkl, usize word_size, NklType target_t, bool is_const) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Pointer);
    HASH_VAL(hasher, usize, word_size);
    HASH_VAL(hasher, intptr_t, target_t);
    HASH_VAL(hasher, bool, is_const);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completePointer(nkl, type, word_size, target_t, is_const);
    }
    return type;
}

static void completeProcedure(NklState nkl, NklType_T *type, usize word_size, NklProcInfo info) {
    NklTypeStorage *st = &nkl->types;

    NklTypeArray param_types_copy = {0};
    NKS_COPY_STRIDED(nk_arena_getAllocator(st->arena), &param_types_copy, info.param_types);

    NklType const base_t = nkl_type_getPointer(nkl, word_size, nkl_type_getVoid(nkl), true);

    *type = (NklType_T){
        ._ir_type = base_t->_ir_type,
        .base_t = base_t,
        .size = base_t->size,
        .align = base_t->align,
        .tclass = NklType_Procedure,
        .as.proc =
            {
                .param_types = param_types_copy,
                .ret_t = info.ret_t,
                .flags = info.flags,
            },
    };
}

NklType nkl_type_getProcedureDistinct(NklState nkl, usize word_size, NklProcInfo info) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeProcedure(nkl, type, word_size, info);
    return type;
}

NklType nkl_type_getProcedure(NklState nkl, usize word_size, NklProcInfo info) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Procedure);
    HASH_VAL(hasher, usize, word_size);
    NK_ITERATE_STRIDED(NklType const *, it, info.param_types) {
        NklType const type = *it;
        HASH_VAL(hasher, intptr_t, type);
    }
    HASH_VAL(hasher, intptr_t, info.ret_t);
    HASH_VAL(hasher, NklProcFlags, info.flags);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeProcedure(nkl, type, word_size, info);
    }
    return type;
}

static void completeStruct(NklState nkl, NklType_T *type, NklFieldStridedArray fields) {
    NklTypeStorage *st = &nkl->types;

    NklFieldArray fields_copy = {0};
    NKS_COPY_STRIDED(nk_arena_getAllocator(st->arena), &fields_copy, fields);

    NklType const base_t = nkl_type_getAggregate(
        nkl,
        (NklTypeStridedArray){
            .strided_data = &fields.strided_data->type,
            .size = fields.size,
            .stride = fields.stride,
        });

    *type = (NklType_T){
        ._ir_type = base_t->_ir_type,
        .base_t = base_t,
        .size = base_t->size,
        .align = base_t->align,
        .tclass = NklType_Struct,
        .as.strct =
            {
                .fields = fields_copy,
            },
    };
}

NklType nkl_type_getStructDistinct(NklState nkl, NklFieldStridedArray fields) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeStruct(nkl, type, fields);
    return type;
}

NklType nkl_type_getStruct(NklState nkl, NklFieldStridedArray fields) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Struct);
    NK_ITERATE_STRIDED(NklField const *, it, fields) {
        HASH_VAL(hasher, NkAtom, it->name);
        HASH_VAL(hasher, intptr_t, it->type);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeStruct(nkl, type, fields);
    }
    return type;
}

static void completeTyperef(NklState NK_UNUSED nkl, NklType_T *type, usize word_size) {
    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .size = word_size,
                .align = word_size,
                .id = 0,
                .kind = NkIrType_Pointer,
            },
        .base_t = NULL,
        .size = word_size,
        .align = word_size,
        .tclass = NklType_Typeref,
    };
}

NklType nkl_type_getTyperefDistinct(NklState nkl, usize word_size) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeTyperef(nkl, type, word_size);
    return type;
}

NklType nkl_type_getTyperef(NklState nkl, usize word_size) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Typeref);
    HASH_VAL(hasher, usize, word_size);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeTyperef(nkl, type, word_size);
    }
    return type;
}

static void completeVoid(NklState NK_UNUSED nkl, NklType_T *type) {
    *type = (NklType_T){
        ._ir_type =
            (NkIrType_T){
                .kind = NkIrType_Aggregate,
            },
        .base_t = NULL,
        .size = 0,
        .align = 0,
        .tclass = NklType_Void,
    };
}

NklType nkl_type_getVoidDistinct(NklState nkl) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeVoid(nkl, type);
    return type;
}

NklType nkl_type_getVoid(NklState nkl) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    HASH_VAL(hasher, NklTypeClass, NklType_Void);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeVoid(nkl, type);
    }
    return type;
}

void nkl_type_inspect(NkStream out, NklType type) {
    switch (type->tclass) {
        break;

        case NklType_Aggregate:
            nk_print(out, "(");
            NK_ITERATE(NklType const *, it, type->as.aggr.types) {
                if (NK_INDEX(it, type->as.aggr.types)) {
                    nk_print(out, ", ");
                }
                NklType elem_t = *it;
                nkl_type_inspect(out, elem_t);
            }
            nk_print(out, ")");
            break;

        case NklType_Array:
            nk_printf(out, "[%zu]", type->as.arr.count);
            nkl_type_inspect(out, type->as.arr.elem_t);
            break;

        case NklType_Bool:
            nk_print(out, "bool");
            break;

        case NklType_Numeric:
            nkir_inspectType(out, nkl_type_toIr(type));
            break;

        case NklType_Pointer:
            nk_printf(out, "*%s", type->as.ptr.is_const ? "const " : "");
            nkl_type_inspect(out, type->as.ptr.target_t);
            break;

        case NklType_Procedure:
            nk_print(out, "*proc (");
            NK_ITERATE(NklType const *, it, type->as.proc.param_types) {
                if (NK_INDEX(it, type->as.proc.param_types)) {
                    nk_print(out, ", ");
                }
                NklType param_t = *it;
                nkl_type_inspect(out, param_t);
            }
            if (type->as.proc.flags & NklProc_Variadic) {
                nk_print(out, ", ...");
            }
            nk_print(out, ") ");
            nkl_type_inspect(out, type->as.proc.ret_t);
            break;

        case NklType_Struct:
            nk_print(out, "struct {");
            NK_ITERATE(NklField const *, it, type->as.strct.fields) {
                if (NK_INDEX(it, type->as.strct.fields)) {
                    nk_print(out, ", ");
                }
                NklField field = *it;
                nk_printf(out, "%s: ", nk_atom2cs(field.name));
                nkl_type_inspect(out, field.type);
            }
            nk_print(out, "}");
            break;

        case NklType_Typeref:
            nk_print(out, "type_t");
            break;

        case NklType_Void:
            nk_print(out, "void");
            break;

        case NklType_None:
        case NklTypeClassCount:
            nk_assert(!"unreachable");
            break;
    }
}
