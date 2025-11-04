#include "nkl/core/types.h"

#include <string.h>

#include "nickl_impl.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/hash.h"
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

#define hashVal(HASHER, T, VAL)                                      \
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

static void completeAggregate(NklState nkl, NklType_T *type, NklTypeStridedArray types) {
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

    *type = (NklType_T){
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

    hashVal(hasher, NklTypeClass, NklType_Aggregate);
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        hashVal(hasher, intptr_t, *it);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeAggregate(nkl, type, types);
    }
    return type;
}

static void completeNumeric(NklState NK_UNUSED nkl, NklType_T *type, NkIrNumericValueType value_type) {
    *type = (NklType_T){
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

    hashVal(hasher, NklTypeClass, NklType_Numeric);
    hashVal(hasher, NkIrNumericValueType, value_type);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeNumeric(nkl, type, value_type);
    }
    return type;
}

static void completeStruct(NklState nkl, NklType_T *type, NklFieldArray fields) {
    NklTypeStorage *st = &nkl->types;

    NklFieldArray fields_copy = {0};
    NKS_COPY(nk_arena_getAllocator(st->arena), &fields_copy, fields);

    NklType const base_t = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_FROM_FIELD(fields, type)});

    *type = (NklType_T){
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

NklType nkl_type_getStructDistinct(NklState nkl, NklFieldArray fields) {
    NklType_T *type = (NklType_T *)nkl_type_getIncomplete(nkl);
    completeStruct(nkl, type, fields);
    return type;
}

NklType nkl_type_getStruct(NklState nkl, NklFieldArray fields) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    hashVal(hasher, NklTypeClass, NklType_Struct);
    NK_ITERATE(NklField const *, it, fields) {
        hashVal(hasher, NkAtom, it->name);
        hashVal(hasher, intptr_t, it->type);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklType_T *type = (NklType_T *)nkl_type_getFromCache(nkl, hash);
    if (!nkl_type_isComplete(type)) {
        completeStruct(nkl, type, fields);
    }
    return type;
}
