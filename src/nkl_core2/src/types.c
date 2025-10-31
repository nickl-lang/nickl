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

NklType nkl_type_getIncomplete(NklState nkl, u64 type_size) {
    NklTypeStorage *st = &nkl->types;
    NklType_T *type = nk_arena_allocAligned(st->arena, type_size, sizeof(max_align_t));
    *type = (NklType_T){
        .type_size = type_size,
    };
    return type;
}

#define getIncompleteT(T, NKL) (T *)nkl_type_getIncomplete((NKL), sizeof(T))

void nkl_type_complete(NklState NK_UNUSED nkl, NklType dst, NklType src) {
    nk_assert(!nkl_type_isComplete(dst));
    nk_assert(nkl_type_isComplete(src));
    nk_assert(dst->type_size >= src->type_size);
    memcpy((void *)dst, src, src->type_size);
}

bool nkl_type_isComplete(NklType type) {
    return type->tclass != NklType_None;
}

NklType nkl_type_getFromCache(NklState nkl, NkHash128 hash, u64 type_size) {
    NklTypeStorage *st = &nkl->types;
    NklType *found = NklTypeMap_find(&st->map, hash);
    if (found) {
        return *found;
    } else {
        NklType const type = nkl_type_getIncomplete(nkl, type_size);
        NklTypeMap_insert(&st->map, hash, type);
        return type;
    }
}

#define getFromCacheT(T, NKL, HASH) (T *)nkl_type_getFromCache((NKL), (HASH), sizeof(T))

static void completeAggregate(NklState nkl, NklAggregateType_T *type, NklTypeStridedArray types) {
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

    *type = (NklAggregateType_T){
        .base =
            {
                .base_t = NULL,
                .size = size,
                .align = align,
                .tclass = NklType_Aggregate,
                .type_size = sizeof(NklAggregateType_T),
            },
        .types = types_copy,
    };
}

NklAggregateType nkl_type_getAggregateDistinct(NklState nkl, NklTypeStridedArray types) {
    NklAggregateType_T *type = getIncompleteT(NklAggregateType_T, nkl);
    completeAggregate(nkl, type, types);
    return type;
}

NklAggregateType nkl_type_getAggregate(NklState nkl, NklTypeStridedArray types) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    hashVal(hasher, NklTypeClass, NklType_Aggregate);
    NK_ITERATE_STRIDED(NklType const *, it, types) {
        hashVal(hasher, intptr_t, *it);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklAggregateType_T *type = getFromCacheT(NklAggregateType_T, nkl, hash);
    if (!nkl_type_isComplete((NklType)type)) {
        completeAggregate(nkl, type, types);
    }
    return type;
}

static void completeNumeric(NklState NK_UNUSED nkl, NklNumericType_T *type, NkIrNumericValueType value_type) {
    *type = (NklNumericType_T){
        .base =
            {
                .base_t = NULL,
                .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
                .tclass = NklType_Numeric,
                .type_size = sizeof(NklNumericType_T),
            },
        .value_type = value_type,
    };
}

NklNumericType nkl_type_getNumericDistinct(NklState nkl, NkIrNumericValueType value_type) {
    NklNumericType_T *type = getIncompleteT(NklNumericType_T, nkl);
    completeNumeric(nkl, type, value_type);
    return type;
}

NklNumericType nkl_type_getNumeric(NklState nkl, NkIrNumericValueType value_type) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    hashVal(hasher, NklTypeClass, NklType_Numeric);
    hashVal(hasher, NkIrNumericValueType, value_type);

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklNumericType_T *type = getFromCacheT(NklNumericType_T, nkl, hash);
    if (!nkl_type_isComplete((NklType)type)) {
        completeNumeric(nkl, type, value_type);
    }
    return type;
}

static void completeStruct(NklState nkl, NklStructType_T *type, NklFieldArray fields) {
    NklTypeStorage *st = &nkl->types;

    NklFieldArray fields_copy = {0};
    NKS_COPY(nk_arena_getAllocator(st->arena), &fields_copy, fields);

    NklType const base_t =
        (NklType)nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_FROM_FIELD(fields, type)});

    *type = (NklStructType_T){
        .base =
            {
                .base_t = base_t,
                .size = base_t->size,
                .align = base_t->align,
                .tclass = NklType_Struct,
                .type_size = sizeof(NklStructType_T),
            },
        .fields = fields_copy,
    };
}

NklStructType nkl_type_getStructDistinct(NklState nkl, NklFieldArray fields) {
    NklStructType_T *type = getIncompleteT(NklStructType_T, nkl);
    completeStruct(nkl, type, fields);
    return type;
}

NklStructType nkl_type_getStruct(NklState nkl, NklFieldArray fields) {
    NkHashState hasher;
    nk_hash128_init(&hasher);

    hashVal(hasher, NklTypeClass, NklType_Struct);
    NK_ITERATE(NklField const *, it, fields) {
        hashVal(hasher, NkAtom, it->name);
        hashVal(hasher, intptr_t, it->type);
    }

    NkHash128 const hash = nk_hash128_finalize(&hasher);

    NklStructType_T *type = getFromCacheT(NklStructType_T, nkl, hash);
    if (!nkl_type_isComplete((NklType)type)) {
        completeStruct(nkl, type, fields);
    }
    return type;
}
