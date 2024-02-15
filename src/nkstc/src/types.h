#ifndef NKSTC_TYPES_H_
#define NKSTC_TYPES_H_

#include "nkb/common.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
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
    bool is_const;
} _nkl_type_pointer;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_slice;

typedef struct {
    NkSlice(NkAtom) fields;
} _nkl_type_struct;

typedef struct NklType_T {
    NkIrType ir_type;
    union {
        _nkl_type_pointer ptr;
        _nkl_type_slice slice;
        _nkl_type_struct strct;
    } as;
    NklTypeClass tclass;
    u32 id;
    nkltype_t underlying_type;
} NklType;

typedef struct {
    void *data;
    nkltype_t type;
} nklval_t;

typedef NkSlice(nkltype_t) NklTypeArray;

typedef struct {
    NklTypeArray args_t;
    nktype_t ret_t;
    NkCallConv call_conv;
    uint8_t flags;
} NklProcInfo;

typedef struct {
    NkAtom name;
    nkltype_t type;
} NklField;

typedef NkSlice(NklField) NklFieldArray;

void nkl_types_init();
void nkl_types_free();

nkltype_t nkl_get_any();
nkltype_t nkl_get_array(nkltype_t elem_type, size_t elem_count);
nkltype_t nkl_get_enum(NklFieldArray fields);
nkltype_t nkl_get_numeric(NkIrNumericValueType value_type);
nkltype_t nkl_get_proc(NklProcInfo info);
nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const);
nkltype_t nkl_get_slice(nkltype_t target_type, bool is_const);
nkltype_t nkl_get_struct(NklFieldArray fields);
nkltype_t nkl_get_struct_packed(NklFieldArray fields);
nkltype_t nkl_get_tuple(NklTypeArray types);
nkltype_t nkl_get_tuple_packed(NklTypeArray types);
nkltype_t nkl_get_typeref();
nkltype_t nkl_get_union(NklFieldArray fields);
nkltype_t nkl_get_void();

void nkl_type_inspect(nkltype_t type, NkStream out);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_TYPES_H_
