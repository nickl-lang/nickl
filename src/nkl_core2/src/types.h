#ifndef NKL_CORE_TYPES_H_
#define NKL_CORE_TYPES_H_

#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/hash.h"
#include "ntk/hash_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NklType_None = 0,

    NklType_Numeric,

    NklTypeClassCount,
};

typedef u32 NklTypeClass;

typedef struct NklType_T const *NklType;

typedef struct NklType_T {
    NklType base_t;
    u64 size;
    u32 align;
    NklTypeClass tclass;
} NklType_T;

// TODO: Move public part of the API to public include folder

// TODO: Hide type storage from public API
NK_HASH_TREE_FWD_KV(NklTypeMap, NkHash128, NklType);

typedef struct {
    NkArena arena;
    NklTypeMap types;
} NklTypeStorage;

// Common {

NklTypeClass nkl_newTypeClass(NklTypeStorage *st);

NklType nkl_type_getIncomplete(NklTypeStorage *st);
void nkl_type_complete(NklTypeStorage *st, NklType dst, NklType src);

NK_INLINE NklType nkl_type_getDistinct(NklTypeStorage *st, NklType type) {
    NklType const res = nkl_type_getIncomplete(st);
    nkl_type_complete(st, res, type);
    return res;
}

NK_INLINE NklType nkl_type_getTypeclassInstance(NklTypeStorage *st, NklTypeClass tclass, NklType base) {
    NklType const res = nkl_type_getIncomplete(st);
    nkl_type_complete(
        st,
        res,
        &(NklType_T){
            .base_t = base,
            .size = base->size,
            .align = base->align,
            .tclass = tclass,
        });
    return res;
}

// }

// Numeric {

typedef struct {
    NklType_T base;
    NkIrNumericValueType value_type;
} NklNumericType;

// TODO: Don't depend on nkb in public API
NklType nkl_type_getNumericDistinct(NklTypeStorage *st, NkIrNumericValueType value_type);
NklType nkl_type_getNumeric(NklTypeStorage *st, NkIrNumericValueType value_type);

// }

// Struct {

typedef struct {
    NkAtom name;
    NklType type;
} NklField;

typedef NkSlice(NklField) NklFieldArray;

typedef struct {
    NklType_T base;
    NklFieldArray fields;
} NklStructType;

NklType nkl_type_getStructDistinct(NklTypeStorage *st, NklFieldArray fields);
NklType nkl_type_getStruct(NklTypeStorage *st, NklFieldArray fields);

// }

// Userland {

NK_INLINE NklType getVectorType(NklTypeStorage *st, NklTypeClass vector_tclass, NklType T) {
    NklField fields[] = {
        {
            .name = nk_cs2atom("x"),
            .type = T,
        },
        {
            .name = nk_cs2atom("y"),
            .type = T,
        },
        {
            .name = nk_cs2atom("z"),
            .type = T,
        },
    };
    NklType const struct_t = nkl_type_getStruct(
        st,
        (NklFieldArray){
            .data = fields,
            .size = NK_ARRAY_COUNT(fields),
        });
    return nkl_type_getTypeclassInstance(st, vector_tclass, struct_t);
}

// }

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_H_
