#ifndef NKL_CORE_TYPES_H_
#define NKL_CORE_TYPES_H_

#include "nkb/common.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklType_T const *nkltype_t;

typedef enum {
    NklType_Any,
    NklType_Array,
    NklType_Enum,
    NklType_Numeric,
    NklType_Pointer,
    NklType_Procedure,
    NklType_Slice,
    NklType_Struct,
    NklType_StructPacked,
    NklType_Tuple,
    NklType_TuplePacked,
    NklType_Typeref,
    NklType_Union,

    NklType_Count,
} NklTypeClass;

typedef struct {
    NkAtom name;
    nkltype_t type;
} NklField;

typedef NkSlice(NklField) NklFieldArray;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_pointer;

typedef struct {
    NklFieldArray fields;
} _nkl_type_struct;

struct NklType_T {
    NkIrType ir_type;
    union {
        _nkl_type_pointer ptr;
        _nkl_type_struct strct;
    } as;
    NklTypeClass tclass;
    u32 id;
    nkltype_t underlying_type;
};

typedef struct {
    void *data;
    nkltype_t type;
} nklval_t;

typedef NkSlice(nkltype_t) NklTypeArray;

typedef struct {
    NklTypeArray param_types;
    nkltype_t ret_t;
    NkCallConv call_conv;
    u8 flags;
} NklProcInfo;

void nkl_types_init(NklState nkl);
void nkl_types_free(NklState nkl);

nkltype_t nkl_get_any(NklState nkl, usize word_size);
nkltype_t nkl_get_array(NklState nkl, nkltype_t elem_type, usize elem_count);
nkltype_t nkl_get_enum(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_numeric(NklState nkl, NkIrNumericValueType value_type);
nkltype_t nkl_get_proc(NklState nkl, usize word_size, NklProcInfo info);
nkltype_t nkl_get_ptr(NklState nkl, usize word_size, nkltype_t target_type, bool is_const);
nkltype_t nkl_get_slice(NklState nkl, usize word_size, nkltype_t target_type, bool is_const);
nkltype_t nkl_get_struct(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_struct_packed(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_tuple(NklState nkl, NklTypeArray types);
nkltype_t nkl_get_tupleEx(NklState nkl, nkltype_t const *types, usize count, usize stride);
nkltype_t nkl_get_tuple_packed(NklState nkl, NklTypeArray types);
nkltype_t nkl_get_typeref(NklState nkl, usize word_size);
nkltype_t nkl_get_union(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_void(NklState nkl);

void nkl_type_inspect(nkltype_t type, NkStream out);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_H_
