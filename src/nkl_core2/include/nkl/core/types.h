#ifndef NKL_CORE_TYPES_H_
#define NKL_CORE_TYPES_H_

#include "nkb/types.h"
#include "nkl/core/nickl.h"
#include "ntk/common.h"
#include "ntk/hash.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NklType_None = 0,

    NklType_Aggregate,
    NklType_Numeric,
    NklType_Struct,

    NklTypeClassCount,
};

typedef u32 NklTypeClass;

typedef struct NklType_T const *NklType;

typedef struct NklType_T {
    NklType base_t;
    u64 size;
    u32 align;
    NklTypeClass tclass;
    u64 type_size;
} NklType_T;

NK_EXPORT NklTypeClass nkl_type_newClass(NklState nkl);

NK_EXPORT NklType nkl_type_getIncomplete(NklState nkl, u64 type_size);
NK_EXPORT void nkl_type_complete(NklState nkl, NklType dst, NklType src);

NK_EXPORT bool nkl_type_isComplete(NklType type);

NK_EXPORT NklType nkl_type_getFromCache(NklState nkl, NkHash128 hash, u64 type_size);

NK_INLINE NklType nkl_type_getDistinct(NklState nkl, NklType type) {
    NklType const res = nkl_type_getIncomplete(nkl, type->type_size);
    nkl_type_complete(nkl, res, type);
    return res;
}

NK_INLINE NklType nkl_type_getTypeclassInstance(NklState nkl, NklTypeClass tclass, NklType base) {
    NklType_T const src_t = {
        .base_t = base,
        .size = base->size,
        .align = base->align,
        .tclass = tclass,
        .type_size = sizeof(NklType_T),
    };
    return nkl_type_getDistinct(nkl, &src_t);
}

typedef NkSlice(NklType const) NklTypeArray;
typedef NkStridedSlice(NklType const) NklTypeStridedArray;

typedef struct NklAggregateType_T const *NklAggregateType;
typedef struct NklAggregateType_T {
    NklType_T base;
    NklTypeArray types;
} NklAggregateType_T;

NK_EXPORT NklAggregateType nkl_type_getAggregateDistinct(NklState nkl, NklTypeStridedArray types);
NK_EXPORT NklAggregateType nkl_type_getAggregate(NklState nkl, NklTypeStridedArray types);

typedef struct NklNumericType_T const *NklNumericType;
typedef struct NklNumericType_T {
    NklType_T base;
    NkIrNumericValueType value_type;
} NklNumericType_T;

// TODO: Don't depend on nkb in public API
NK_EXPORT NklNumericType nkl_type_getNumericDistinct(NklState nkl, NkIrNumericValueType value_type);
NK_EXPORT NklNumericType nkl_type_getNumeric(NklState nkl, NkIrNumericValueType value_type);

typedef struct {
    NkAtom name;
    NklType type;
} NklField;

typedef NkSlice(NklField const) NklFieldArray;

typedef struct NklStructType_T const *NklStructType;
typedef struct NklStructType_T {
    NklType_T base;
    NklFieldArray fields;
} NklStructType_T;

NK_EXPORT NklStructType nkl_type_getStructDistinct(NklState nkl, NklFieldArray fields);
NK_EXPORT NklStructType nkl_type_getStruct(NklState nkl, NklFieldArray fields);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_H_
