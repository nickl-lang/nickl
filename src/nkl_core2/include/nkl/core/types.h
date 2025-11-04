#ifndef NKL_CORE_TYPES_H_
#define NKL_CORE_TYPES_H_

// TODO: Don't depend on nkb in public API
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
    NklType_Array,
    NklType_Bool,
    NklType_Numeric,
    NklType_Pointer,
    NklType_Procedure,
    NklType_Struct,
    NklType_Void,

    NklTypeClassCount,
};

typedef u32 NklTypeClass;

typedef struct NklType_T const *NklType;

typedef NkSlice(NklType const) NklTypeArray;
typedef NkStridedSlice(NklType const) NklTypeStridedArray;

typedef enum {
    NklProcFlags_Variadic = 1 << 0,
} NklProcFlags;

typedef struct {
    NkAtom name;
    NklType type;
} NklField;

typedef NkSlice(NklField const) NklFieldArray;
typedef NkStridedSlice(NklField const) NklFieldStridedArray;

typedef struct NklType_T {
    NklType base_t;
    u64 size;
    u32 align;
    NklTypeClass tclass;
    union {
        struct {
            NklType elem_t;
            size_t count;
        } arr;
        struct {
            NklTypeArray types;
        } aggr;
        struct {
            NkIrNumericValueType value_type;
        } num;
        struct {
            NklType target_t;
            bool is_const;
        } ptr;
        struct {
            NklTypeArray param_types;
            NklType ret_t;
            NklProcFlags flags;
        } proc;
        struct {
            NklFieldArray fields;
        } strct;
    } as;
} NklType_T;

NK_EXPORT NklTypeClass nkl_type_newClass(NklState nkl);

NK_EXPORT NklType nkl_type_getIncomplete(NklState nkl);
NK_EXPORT void nkl_type_complete(NklState nkl, NklType dst, NklType src);

NK_EXPORT bool nkl_type_isComplete(NklType type);

NK_EXPORT NklType nkl_type_getFromCache(NklState nkl, NkHash128 hash);

NK_INLINE NklType nkl_type_getDistinct(NklState nkl, NklType type) {
    NklType const res = nkl_type_getIncomplete(nkl);
    nkl_type_complete(nkl, res, type);
    return res;
}

NK_EXPORT NklType nkl_type_getTypeclassInstance(NklState nkl, NklTypeClass tclass, NklType base);

NK_EXPORT NklType nkl_type_getAggregateDistinct(NklState nkl, NklTypeStridedArray types);
NK_EXPORT NklType nkl_type_getAggregate(NklState nkl, NklTypeStridedArray types);

NK_EXPORT NklType nkl_type_getArrayDistinct(NklState nkl, NklType elem_t, usize count);
NK_EXPORT NklType nkl_type_getArray(NklState nkl, NklType elem_t, usize count);

NK_EXPORT NklType nkl_type_getBoolDistinct(NklState nkl);
NK_EXPORT NklType nkl_type_getBool(NklState nkl);

NK_EXPORT NklType nkl_type_getNumericDistinct(NklState nkl, NkIrNumericValueType value_type);
NK_EXPORT NklType nkl_type_getNumeric(NklState nkl, NkIrNumericValueType value_type);

NK_EXPORT NklType nkl_type_getPointerDistinct(NklState nkl, NkIrNumericValueType ptr, NklType target_t, bool is_const);
NK_EXPORT NklType nkl_type_getPointer(NklState nkl, NkIrNumericValueType ptr, NklType target_t, bool is_const);

typedef struct {
    NklTypeStridedArray param_types;
    NklType ret_t;
    u8 flags;
} NklProcInfo;

NK_EXPORT NklType nkl_type_getProcedureDistinct(NklState nkl, NkIrNumericValueType ptr, NklProcInfo info);
NK_EXPORT NklType nkl_type_getProcedure(NklState nkl, NkIrNumericValueType ptr, NklProcInfo info);

NK_EXPORT NklType nkl_type_getStructDistinct(NklState nkl, NklFieldStridedArray fields);
NK_EXPORT NklType nkl_type_getStruct(NklState nkl, NklFieldStridedArray fields);

NK_EXPORT NklType nkl_type_getVoidDistinct(NklState nkl);
NK_EXPORT NklType nkl_type_getVoid(NklState nkl);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_H_
