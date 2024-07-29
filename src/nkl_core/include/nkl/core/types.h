#ifndef NKL_CORE_TYPES_H_
#define NKL_CORE_TYPES_H_

#include "nkb/common.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklType_T const *nkltype_t;

typedef enum {
    NklType_None,

    NklType_Any,
    NklType_Array,
    NklType_Bool,
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

typedef NkStridedSlice(nkltype_t) NklTypeStridedArray;

typedef struct {
    NklTypeStridedArray param_types;
    nkltype_t ret_t;
    NkCallConv call_conv;
    u8 flags;
} NklProcInfo;

void nkl_types_init(NklState nkl);
void nkl_types_free(NklState nkl);

nkltype_t nkl_get_any(NklState nkl, usize word_size);
nkltype_t nkl_get_array(NklState nkl, nkltype_t elem_type, usize elem_count);
nkltype_t nkl_get_bool(NklState nkl);
nkltype_t nkl_get_enum(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_numeric(NklState nkl, NkIrNumericValueType value_type);
nkltype_t nkl_get_int(NklState nkl, usize size, bool is_signed);
nkltype_t nkl_get_proc(NklState nkl, usize word_size, NklProcInfo info);
nkltype_t nkl_get_ptr(NklState nkl, usize word_size, nkltype_t target_type, bool is_const);
nkltype_t nkl_get_slice(NklState nkl, usize word_size, nkltype_t target_type, bool is_const);
nkltype_t nkl_get_struct(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_struct_packed(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_tuple(NklState nkl, NklTypeStridedArray types);
nkltype_t nkl_get_tupleEx(NklState nkl, nkltype_t const *types, usize count, usize stride);
nkltype_t nkl_get_tuple_packed(NklState nkl, NklTypeStridedArray types);
nkltype_t nkl_get_typeref(NklState nkl, usize word_size);
nkltype_t nkl_get_union(NklState nkl, NklFieldArray fields);
nkltype_t nkl_get_void(NklState nkl);

void nkl_type_inspect(nkltype_t type, NkStream out);

NK_INLINE nktype_t nklt2nkirt(nkltype_t type) {
    return &type->ir_type;
}

NK_INLINE nkltype_t nkirt2nklt(nktype_t type) {
    return (nkltype_t)type;
}

NK_INLINE void *nklval_data(nklval_t val) {
    return val.data;
}

NK_INLINE nkltype_t nklval_typeof(nklval_t val) {
    return val.type;
}

NK_INLINE u32 nklt_typeid(nkltype_t type) {
    return type->id;
}

NK_INLINE NklTypeClass nklt_tclass(nkltype_t type) {
    return type->tclass;
}

NK_INLINE u64 nklt_sizeof(nkltype_t type) {
    return nklt2nkirt(type)->size;
}

#define nklval_as(TYPE, VAL) (*(TYPE *)nklval_data(VAL))

NK_INLINE nkltype_t nklt_underlying(nkltype_t type) {
    return type->underlying_type;
}

NK_INLINE usize nklt_array_size(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Array);
    return nklt2nkirt(type)->as.aggr.elems.data[0].count;
}

NK_INLINE nkltype_t nklt_array_elemType(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Array);
    return nkirt2nklt(nklt2nkirt(type)->as.aggr.elems.data[0].type);
}

NK_INLINE usize nklt_proc_paramCount(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Procedure);
    return nklt2nkirt(type)->as.proc.info.args_t.size;
}

NK_INLINE nkltype_t nklt_proc_paramType(nkltype_t type, usize idx) {
    nk_assert(nklt_tclass(type) == NklType_Procedure);
    NkTypeArray param_types = nklt2nkirt(type)->as.proc.info.args_t;
    nk_assert(idx < param_types.size && "index out of range");
    return nkirt2nklt(param_types.data[idx]);
}

NK_INLINE nkltype_t nklt_proc_retType(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Procedure);
    return nkirt2nklt(nklt2nkirt(type)->as.proc.info.ret_t);
}

NK_INLINE NkCallConv nklt_proc_callConv(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Procedure);
    return nklt2nkirt(type)->as.proc.info.call_conv;
}

NK_INLINE u8 nklt_proc_flags(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Procedure);
    return nklt2nkirt(type)->as.proc.info.flags;
}

NK_INLINE NkIrNumericValueType nklt_numeric_valueType(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Numeric);
    return nklt2nkirt(type)->as.num.value_type;
}

NK_INLINE nkltype_t nklt_ptr_target(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Pointer);
    return type->as.ptr.target_type;
}

NK_INLINE bool nklt_ptr_isConst(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Pointer);
    return type->as.ptr.is_const;
}

NK_INLINE NklField nklt_struct_field(nkltype_t type, usize idx) {
    nk_assert(nklt_tclass(type) == NklType_Struct);
    nk_assert(idx < type->as.strct.fields.size && "index out of range");
    return type->as.strct.fields.data[idx];
}

NK_INLINE usize nklt_struct_index(nkltype_t type, NkAtom name) {
    nk_assert(nklt_tclass(type) == NklType_Struct);
    for (usize i = 0; i < type->as.strct.fields.size; i++) {
        if (name == type->as.strct.fields.data[i].name) {
            return i;
        }
    }
    return -1u;
}

NK_INLINE nkltype_t nklt_slice_target(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Slice);
    return nklt_ptr_target(nklt_struct_field(nklt_underlying(type), 0).type);
}

NK_INLINE usize nklt_tuple_size(nkltype_t type) {
    nk_assert(nklt_tclass(type) == NklType_Tuple);
    return nklt2nkirt(type)->as.aggr.elems.size;
}

NK_INLINE nkltype_t nklt_tuple_elemType(nkltype_t type, usize idx) {
    nk_assert(nklt_tclass(type) == NklType_Tuple);
    nk_assert(idx < nklt2nkirt(type)->as.aggr.elems.size && "index out of range");
    return nkirt2nklt(nklt2nkirt(type)->as.aggr.elems.data[idx].type);
}

NK_INLINE usize nklt_tuple_elemOffset(nkltype_t type, usize idx) {
    nk_assert(nklt_tclass(type) == NklType_Tuple);
    nk_assert(idx < nklt2nkirt(type)->as.aggr.elems.size && "index out of range");
    return nklt2nkirt(type)->as.aggr.elems.data[idx].offset;
}

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_TYPES_H_
