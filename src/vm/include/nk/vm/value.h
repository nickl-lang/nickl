#ifndef HEADER_GUARD_NK_VM_VALUE
#define HEADER_GUARD_NK_VM_VALUE

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "nk/common/allocator.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"
#include "nk/vm/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NkType_Array,
    NkType_Fn,
    NkType_Numeric,
    NkType_Ptr,
    NkType_Tuple,
    NkType_Void,

    NkTypeclass_Count,
} NkTypeclassId;

typedef struct {
    nktype_t elem_type;
    size_t elem_count;
} _nk_type_array;

typedef enum {
    NkCallConv_Nk,
    NkCallConv_Cdecl,
} NkCallConv;

typedef struct {
    nktype_t ret_t;
    nktype_t args_t;
    NkCallConv call_conv;
    bool is_variadic;
} _nk_type_fn;

// 0x<index><size>
// Ordered in coercion order, max(lhs, rhs) is a common type
typedef enum {
    Int8 = 0x01,
    Uint8 = 0x11,
    Int16 = 0x22,
    Uint16 = 0x32,
    Int32 = 0x44,
    Uint32 = 0x54,
    Int64 = 0x68,
    Uint64 = 0x78,
    Float32 = 0x84,
    Float64 = 0x98,
} NkNumericValueType;

#define NUM_TYPE_SIZE(value_type) (0xf & (value_type))
#define NUM_TYPE_INDEX(value_type) ((0xf0 & (value_type)) >> 4)

typedef struct {
    NkNumericValueType value_type;
} _nk_type_numeric;

typedef struct {
    nktype_t target_type;
} _nk_type_ptr;

typedef struct {
    nktype_t type;
    size_t offset;
} NkTupleElemInfo;

typedef struct {
    NkTupleElemInfo *data;
    size_t size;
} NkTupleElemInfoArray;

typedef struct {
    NkTupleElemInfoArray elems;
} _nk_type_tuple;

typedef struct NkType {
    union {
        _nk_type_array arr;
        _nk_type_fn fn;
        _nk_type_numeric num;
        _nk_type_ptr ptr;
        _nk_type_tuple tuple;
    } as;
    uint64_t size;
    uint8_t align;
    nk_typeclassid_t tclass;
} NkType;

NkType nkt_get_array(nktype_t elem_type, size_t elem_count);

typedef struct {
    nktype_t ret_t;
    nktype_t args_t;
    NkCallConv call_conv;
    bool is_variadic;
} NktFnInfo;

NkType nkt_get_fn(NktFnInfo info);

NkType nkt_get_numeric(NkNumericValueType value_type);

NkType nkt_get_ptr(nktype_t target_type);

//@Feature TODO Implement optimized tuple type
NkType nkt_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride);

NkType nkt_get_void();

void nkt_inspect(nktype_t type, NkStringBuilder sb);
void nkval_inspect(nkval_t val, NkStringBuilder sb);

void nkval_fn_invoke(nkval_t fn, nkval_t ret, nkval_t args);

size_t nkval_array_size(nkval_t self);
nkval_t nkval_array_at(nkval_t self, size_t i);

size_t nkval_tuple_size(nkval_t self);
nkval_t nkval_tuple_at(nkval_t self, size_t i);

typedef struct {
    NkTupleElemInfoArray info_ar;
    size_t size;
    size_t align;
} NkTupleLayout;

NkTupleLayout nk_calcTupleLayout(
    nktype_t const *types,
    size_t count,
    NkAllocator allocator,
    size_t stride);

inline nkval_t nkval_undefined() {
    return nkval_t{};
}

inline void *nkval_data(nkval_t val) {
    return val.data;
}

inline nktype_t nkval_typeof(nkval_t val) {
    return val.type;
}

inline nk_typeclassid_t nkval_typeclassid(nkval_t val) {
    return nkval_typeof(val)->tclass;
}

inline size_t nkval_sizeof(nkval_t val) {
    return nkval_typeof(val)->size;
}

inline size_t nkval_alignof(nkval_t val) {
    return nkval_typeof(val)->align;
}

inline nkval_t nkval_reinterpret_cast(nktype_t type, nkval_t val) {
    return nkval_t{nkval_data(val), type};
}

inline nkval_t nkval_copy(void *dst, nkval_t src) {
    memcpy(dst, nkval_data(src), nkval_sizeof(src));
    return nkval_t{dst, nkval_typeof(src)};
}

#define nkval_as(TYPE, VAL) (*(TYPE *)nkval_data(VAL))

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_VALUE
