#ifndef NKL_LANG_VALUE_H_
#define NKL_LANG_VALUE_H_

#include <string.h>

#include "nk/vm/common.h"
#include "nk/vm/value.h"
#include "ntk/atom.h"
#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklType_Typeref = NkTypeclass_Count,
    NklType_Any,
    NklType_Slice,
    NklType_Struct,
    NklType_Union,
    NklType_Enum,

    NklTypeclass_Count,
} NklTypeclassId;

typedef struct NklType const *nkltype_t;

typedef u64 nkl_typeid_t;
typedef u8 nkl_typeclassid_t;

typedef struct {
    nkltype_t elem_type;
    usize elem_count;
} _nkl_type_array;

typedef struct {
    nkltype_t ret_t;
    nkltype_t args_t;
    NkCallConv call_conv;
    bool is_variadic;
} _nkl_type_fn;

typedef _nkl_type_fn NkltFnInfo;

typedef struct {
    NkNumericValueType value_type;
} _nkl_type_numeric;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_ptr;

typedef struct {
    nkltype_t type;
    usize offset;
} NklTupleElemInfo;

typedef struct {
    NklTupleElemInfo *data;
    usize size;
} NklTupleElemInfoArray;

typedef struct {
    NklTupleElemInfoArray elems;
} _nkl_type_tuple;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_slice;

typedef struct {
    NkAtom name;
    nkltype_t type;
} NklField;

typedef struct {
    NklField const *data;
    usize size;
} NklFieldArray;

typedef struct {
    NklFieldArray fields;
} _nkl_type_struct;

typedef struct NklType {
    NkType vm_type;
    union {
        _nkl_type_array arr;
        _nkl_type_fn fn;
        _nkl_type_numeric num;
        _nkl_type_ptr ptr;
        _nkl_type_tuple tuple;
        _nkl_type_slice slice;
        _nkl_type_struct strct;
    } as;
    nkl_typeclassid_t tclass;
    nkl_typeid_t id;
    nkltype_t underlying_type = nullptr;
} NklType;

typedef struct {
    void *data;
    nkltype_t type;
} nklval_t;

typedef struct {
    nkltype_t const *data;
    usize size;
} NklTypeArray;

void nkl_types_clean();

nkltype_t nkl_get_array(nkltype_t elem_type, usize elem_count);
nkltype_t nkl_get_fn(NkltFnInfo info);
nkltype_t nkl_get_numeric(NkNumericValueType value_type);
nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const = false);
nkltype_t nkl_get_tuple(NkAllocator alloc, NklTypeArray types, usize stride);
nkltype_t nkl_get_void();

nkltype_t nkl_get_typeref();
nkltype_t nkl_get_any(NkAllocator alloc);
nkltype_t nkl_get_slice(NkAllocator alloc, nkltype_t elem_type, bool is_const = false);
nkltype_t nkl_get_struct(NkAllocator alloc, NklFieldArray fields);
nkltype_t nkl_get_union(NkAllocator alloc, NklFieldArray fields);
nkltype_t nkl_get_enum(NkAllocator alloc, NklFieldArray fields);

void nklt_inspect(nkltype_t type, NkStringBuilder *sb);
void nklval_inspect(nklval_t val, NkStringBuilder *sb);

usize nklt_struct_index(nkltype_t type, NkAtom name);

void nklval_fn_invoke(nklval_t fn, nklval_t ret, nklval_t args);

usize nklval_array_size(nklval_t self);
nklval_t nklval_array_at(nklval_t self, usize i);

usize nklval_tuple_size(nklval_t self);
nklval_t nklval_tuple_at(nklval_t self, usize i);

nklval_t nklval_struct_at(nklval_t val, NkAtom name);

inline nklval_t nklval_undefined() {
    return nklval_t{};
}

inline void *nklval_data(nklval_t val) {
    return val.data;
}

inline nkltype_t nklval_typeof(nklval_t val) {
    return val.type;
}

inline nkl_typeid_t nklt_typeid(nkltype_t type) {
    return type->id;
}

inline nkl_typeclassid_t nklt_tclass(nkltype_t type) {
    return type->tclass;
}

inline nktype_t tovmt(nkltype_t type) {
    return &type->vm_type;
}

inline nkval_t tovmv(nklval_t val) {
    return {nklval_data(val), tovmt(nklval_typeof(val))};
}

inline NktFnInfo tovmf(NkltFnInfo info) {
    return NktFnInfo{
        .ret_t = tovmt(info.ret_t),
        .args_t = tovmt(info.args_t),
        .call_conv = info.call_conv,
        .is_variadic = info.is_variadic,
    };
}

inline nkltype_t fromvmt(nktype_t type) {
    return (nkltype_t)type;
}

inline nklval_t fromvmv(nkval_t val) {
    return {nkval_data(val), fromvmt(nkval_typeof(val))};
}

inline NkltFnInfo fromvmf(NktFnInfo info) {
    return NkltFnInfo{
        .ret_t = fromvmt(info.ret_t),
        .args_t = fromvmt(info.args_t),
        .call_conv = info.call_conv,
        .is_variadic = info.is_variadic,
    };
}

inline usize nklt_sizeof(nkltype_t type) {
    return nkt_sizeof(tovmt(type));
}

inline usize nklt_alignof(nkltype_t type) {
    return nkt_alignof(tovmt(type));
}

inline nkl_typeid_t nklval_typeid(nklval_t val) {
    return nklt_typeid(nklval_typeof(val));
}

inline nkl_typeclassid_t nklval_tclass(nklval_t val) {
    return nklt_tclass(nklval_typeof(val));
}

inline usize nklval_sizeof(nklval_t val) {
    return nklt_sizeof(nklval_typeof(val));
}

inline usize nklval_alignof(nklval_t val) {
    return nklt_alignof(nklval_typeof(val));
}

inline nklval_t nklval_reinterpret_cast(nkltype_t type, nklval_t val) {
    return nklval_t{nklval_data(val), type};
}

inline nklval_t nklval_copy(void *dst, nklval_t src) {
    memcpy(dst, nklval_data(src), nklval_sizeof(src));
    return nklval_t{dst, nklval_typeof(src)};
}

#define nklval_as(TYPE, VAL) (*(TYPE *)nklval_data(VAL))

#ifdef __cplusplus
}
#endif

#endif // NKL_LANG_VALUE_H_
