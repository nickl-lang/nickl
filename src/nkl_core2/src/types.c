#include "nkl/core/types.h"

#include <string.h>

#include "nickl_impl.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/hash.h"
#include "ntk/hash_tree.h"
#include "ntk/utils.h"

// TODO: Use 128bit hash directly?
NkHash64 NkHash128_KeyHash(NkHash128 key) {
    return *(NkHash64 *)key.bytes;
}

// NkHash128 const *NklType_T_GetKey(NklType item) {
//     return &item->key;
// }

NK_HASH_TREE_IMPL_KV(NklTypeMap, NkHash128, NklType, NkHash128_KeyHash, nk_hash128_equal);
// NK_HASH_TREE_IMPL(NklTypeMap, NklType_T, NkHash128, NklType_T_GetKey, NkHash128_KeyHash, nk_hash128_equal);

void nkl_types_init(NklTypeStorage *st, NkArena *arena) {
    *st = (NklTypeStorage){
        .arena = arena,
        .map = (NklTypeMap){.alloc = nk_arena_getAllocator(arena)},
        .next_tclass = NklTypeClassCount,
    };
}

NklTypeClass nkl_newTypeClass(NklState nkl) {
    return nkl->types.next_tclass++;
}

NklType nkl_type_getIncomplete(NklState nkl, u64 type_size) {
    NklTypeStorage *st = &nkl->types;
    NklType_T *type = nk_arena_allocAligned(st->arena, type_size, sizeof(max_align_t));
    *type = (NklType_T){0};
    return type;
}

void nkl_type_complete(NklState NK_UNUSED nkl, NklType dst, NklType src) {
    *(NklType_T *)dst = *src;
}

NklAggregateType nkl_type_getAggregateDistinct(NklState nkl, NklTypeStridedArray types) {
    NklTypeStorage *st = &nkl->types;

    NklTypeArray types_copy = {0};
    NKS_COPY_STRIDED(nk_arena_getAllocator(st->arena), &types_copy, types);

    usize align = 0;
    usize offset = 0;
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        NklType const type = *it;

        align = nk_maxu(align, type->align);
        offset = nk_alignToPowerOf2(offset, type->align) + type->size;
    }
    usize const size = nk_alignToPowerOf2(offset, align);

    NklAggregateType_T *type = nk_arena_allocT(st->arena, NklAggregateType_T);
    *type = (NklAggregateType_T){
        .base =
            {
                .base_t = NULL,
                .size = size,
                .align = align,
                .tclass = NklType_Aggregate,
            },
        .types = types_copy,
    };
    return type;
}

NklAggregateType nkl_type_getAggregate(NklState nkl, NklTypeStridedArray types) {
    // TODO: Reuse caching code between types
    NkHashState hasher;
    nk_hash128_init(&hasher);
    NklTypeClass const tclass = NklType_Aggregate;
    nk_hash128_update(&hasher, (u8 const *)&tclass, sizeof(tclass));
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        nk_hash128_update(&hasher, (u8 const *)it, sizeof(NklType));
    }
    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklTypeStorage *st = &nkl->types;
    NklType *found = NklTypeMap_find(&st->map, hash);
    if (found) {
        return (NklAggregateType)*found;
    } else {
        NklAggregateType type = nkl_type_getAggregateDistinct(nkl, types);
        NklTypeMap_insert(&st->map, hash, (NklType)type);
        return type;
    }
}

NklNumericType nkl_type_getNumericDistinct(NklState nkl, NkIrNumericValueType value_type) {
    NklTypeStorage *st = &nkl->types;
    NklNumericType_T *type = nk_arena_allocT(st->arena, NklNumericType_T);
    *type = (NklNumericType_T){
        .base =
            {
                .base_t = NULL,
                .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .tclass = NklType_Numeric,
            },
        .value_type = value_type,
    };
    return type;
}

NklNumericType nkl_type_getNumeric(NklState nkl, NkIrNumericValueType value_type) {
    // TODO: Reuse caching code between types
    NkHashState hasher;
    nk_hash128_init(&hasher);
    NklTypeClass const tclass = NklType_Numeric;
    nk_hash128_update(&hasher, (u8 const *)&tclass, sizeof(tclass));
    nk_hash128_update(&hasher, (u8 const *)&value_type, sizeof(value_type));
    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklTypeStorage *st = &nkl->types;
    NklType *found = NklTypeMap_find(&st->map, hash);
    if (found) {
        return (NklNumericType)*found;
    } else {
        NklNumericType type = nkl_type_getNumericDistinct(nkl, value_type);
        NklTypeMap_insert(&st->map, hash, (NklType)type);
        return type;
    }
}

NklStructType nkl_type_getStructDistinct(NklState nkl, NklFieldArray fields) {
    NklTypeStorage *st = &nkl->types;

    NklFieldArray fields_copy = {0};
    NKS_COPY(nk_arena_getAllocator(st->arena), &fields_copy, fields);

    NklType const base_t =
        (NklType)nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_FROM_FIELD(fields, type)});

    NklStructType_T *type = nk_arena_allocT(st->arena, NklStructType_T);
    *type = (NklStructType_T){
        .base =
            {
                .base_t = base_t,
                .size = base_t->size,
                .align = base_t->align,
                .tclass = NklType_Struct,
            },
        .fields = fields_copy,
    };
    return type;
}

NklStructType nkl_type_getStruct(NklState nkl, NklFieldArray fields) {
    // TODO: Reuse caching code between types
    NkHashState hasher;
    nk_hash128_init(&hasher);
    NklTypeClass const tclass = NklType_Struct;
    nk_hash128_update(&hasher, (u8 const *)&tclass, sizeof(tclass));
    NK_ITERATE(NklField const *, it, fields) {
        nk_hash128_update(&hasher, (u8 const *)&it->name, sizeof(it->name));
        nk_hash128_update(&hasher, (u8 const *)&it->type, sizeof((intptr_t)it->type));
    }
    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklTypeStorage *st = &nkl->types;
    NklType *found = NklTypeMap_find(&st->map, hash);
    if (found) {
        return (NklStructType)*found;
    } else {
        NklStructType type = nkl_type_getStructDistinct(nkl, fields);
        NklTypeMap_insert(&st->map, hash, (NklType)type);
        return type;
    }
}
