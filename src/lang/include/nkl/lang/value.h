#ifndef HEADER_GUARD_NKL_LANG_VALUE
#define HEADER_GUARD_NKL_LANG_VALUE

#include <stdint.h>

#include "nk/common/common.h"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklType_Slice = NkTypeclass_Count,

    NklTypeclass_Count,
} NklTypeclassId;

typedef struct NklType const *nkltype_t;

typedef uint64_t nkl_typeid_t;
typedef uint8_t nkl_typeclassid_t;

typedef struct {
    nkltype_t elem_type;
    size_t elem_count;
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
    size_t offset;
} NklTupleElemInfo;

typedef struct {
    NklTupleElemInfo *data;
    size_t size;
} NklTupleElemInfoArray;

typedef struct {
    NklTupleElemInfoArray elems;
} _nkl_type_tuple;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_slice;

typedef struct NklType {
    NkType vm_type;
    union {
        _nkl_type_array arr;
        _nkl_type_fn fn;
        _nkl_type_numeric num;
        _nkl_type_ptr ptr;
        _nkl_type_tuple tuple;
        _nkl_type_slice slice;
    } as;
    nkl_typeclassid_t tclass;
    nkl_typeid_t id;
} NklType;

typedef struct {
    void *data;
    nkltype_t type;
} nklval_t;

NK_EXPORT void nkl_types_clean();

NK_EXPORT nkltype_t nkl_get_array(nkltype_t elem_type, size_t elem_count);
NK_EXPORT nkltype_t nkl_get_fn(NkltFnInfo info);
NK_EXPORT nkltype_t nkl_get_numeric(NkNumericValueType value_type);
NK_EXPORT nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const = false);
NK_EXPORT nkltype_t
nkl_get_tuple(NkAllocator alloc, nkltype_t const *types, size_t count, size_t stride);
NK_EXPORT nkltype_t nkl_get_void();

NK_EXPORT nkltype_t nkl_get_slice(NkAllocator alloc, nkltype_t elem_type, bool is_const = false);

void nklt_inspect(nkltype_t type, NkStringBuilder sb);
void nklval_inspect(nklval_t val, NkStringBuilder sb);

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

inline size_t nklt_sizeof(nkltype_t type) {
    return nkt_sizeof(tovmt(type));
}

inline size_t nklt_alignof(nkltype_t type) {
    return nkt_alignof(tovmt(type));
}

inline nkl_typeid_t nklval_typeid(nklval_t val) {
    return nklt_typeid(nklval_typeof(val));
}

inline nkl_typeclassid_t nklval_tclass(nklval_t val) {
    return nklt_tclass(nklval_typeof(val));
}

inline size_t nklval_sizeof(nklval_t val) {
    return nklt_sizeof(nklval_typeof(val));
}

inline size_t nklval_alignof(nklval_t val) {
    return nklt_alignof(nklval_typeof(val));
}

inline nklval_t nklval_reinterpret_cast(nkltype_t type, nklval_t val) {
    return nklval_t{nklval_data(val), type};
}

#define nklval_as(TYPE, VAL) (*(TYPE *)nklval_data(VAL))

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_VALUE
