#ifndef NK_VM_VALUE_H_
#define NK_VM_VALUE_H_

#include <string.h>

#include "nk/vm/common.h"
#include "ntk/allocator.h"
#include "ntk/common.h"
#include "ntk/string_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u64 nk_typeid_t;
typedef u8 nk_typeclassid_t;

typedef enum {
    NkType_Array,
    NkType_Fn,
    NkType_Numeric,
    NkType_Ptr,
    NkType_Tuple,

    NkTypeclass_Count,
} NkTypeclassId;

typedef struct {
    nktype_t elem_type;
    usize elem_count;
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

typedef _nk_type_fn NktFnInfo;

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

#define NUM_TYPE_SIZE(VALUE_TYPE) (0xf & (VALUE_TYPE))
#define NUM_TYPE_INDEX(VALUE_TYPE) ((0xf0 & (VALUE_TYPE)) >> 4)
#define NUM_TYPE_COUNT 10

#define NUMERIC_ITERATE_INT(MACRO, ...)           \
    MACRO(i8, Int8 __VA_OPT__(, ) __VA_ARGS__)    \
    MACRO(u8, Uint8 __VA_OPT__(, ) __VA_ARGS__)   \
    MACRO(i16, Int16 __VA_OPT__(, ) __VA_ARGS__)  \
    MACRO(u16, Uint16 __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(i32, Int32 __VA_OPT__(, ) __VA_ARGS__)  \
    MACRO(u32, Uint32 __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(i64, Int64 __VA_OPT__(, ) __VA_ARGS__)  \
    MACRO(u64, Uint64 __VA_OPT__(, ) __VA_ARGS__)

#define NUMERIC_ITERATE(MACRO, ...)                       \
    NUMERIC_ITERATE_INT(MACRO __VA_OPT__(, ) __VA_ARGS__) \
    MACRO(f32, Float32 __VA_OPT__(, ) __VA_ARGS__)        \
    MACRO(f64, Float64 __VA_OPT__(, ) __VA_ARGS__)

typedef struct {
    NkNumericValueType value_type;
} _nk_type_numeric;

typedef struct {
    nktype_t target_type;
} _nk_type_ptr;

typedef struct {
    nktype_t type;
    usize offset;
} NkTupleElemInfo;

typedef struct {
    NkTupleElemInfo *data;
    usize size;
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
    u64 size;
    u8 align;
    nk_typeclassid_t tclass;
    nk_typeid_t id;
} NkType;

NkType nkt_get_array(nktype_t elem_type, usize elem_count);

NkType nkt_get_fn(NktFnInfo info);

NkType nkt_get_numeric(NkNumericValueType value_type);

NkType nkt_get_ptr(nktype_t target_type);

//@Feature TODO Implement optimized tuple type
NkType nkt_get_tuple(NkAllocator alloc, nktype_t const *types, usize count, usize stride);

NkType nkt_get_void();

void nkt_inspect(nktype_t type, NkStringBuilder *sb);
void nkval_inspect(nkval_t val, NkStringBuilder *sb);

void nkval_fn_invoke(nkval_t fn, nkval_t ret, nkval_t args);

usize nkval_array_size(nkval_t self);
nkval_t nkval_array_at(nkval_t self, usize i);

usize nkval_tuple_size(nkval_t self);
nkval_t nkval_tuple_at(nkval_t self, usize i);

typedef struct {
    NkTupleElemInfoArray info_ar;
    usize size;
    usize align;
} NkTupleLayout;

NkTupleLayout nk_calcTupleLayout(nktype_t const *types, usize count, NkAllocator allocator, usize stride);

inline nkval_t nkval_undefined() {
    return NK_LITERAL(nkval_t) NK_ZERO_STRUCT;
}

inline void *nkval_data(nkval_t val) {
    return val.data;
}

inline nktype_t nkval_typeof(nkval_t val) {
    return val.type;
}

inline nk_typeid_t nkt_typeid(nktype_t type) {
    return type->id;
}

inline nk_typeclassid_t nkt_typeclassid(nktype_t type) {
    return type->tclass;
}

inline usize nkt_sizeof(nktype_t type) {
    return type->size;
}

inline usize nkt_alignof(nktype_t type) {
    return type->align;
}

inline nk_typeid_t nkval_typeid(nkval_t val) {
    return nkt_typeid(nkval_typeof(val));
}

inline nk_typeclassid_t nkval_typeclassid(nkval_t val) {
    return nkt_typeclassid(nkval_typeof(val));
}

inline usize nkval_sizeof(nkval_t val) {
    return nkt_sizeof(nkval_typeof(val));
}

inline usize nkval_alignof(nkval_t val) {
    return nkt_alignof(nkval_typeof(val));
}

inline nkval_t nkval_reinterpret_cast(nktype_t type, nkval_t val) {
    return NK_LITERAL(nkval_t){nkval_data(val), type};
}

inline nkval_t nkval_copy(void *dst, nkval_t src) {
    memcpy(dst, nkval_data(src), nkval_sizeof(src));
    return NK_LITERAL(nkval_t){dst, nkval_typeof(src)};
}

#define nkval_as(TYPE, VAL) (*(TYPE *)nkval_data(VAL))

#ifdef __cplusplus
}
#endif

#endif // NK_VM_VALUE_H_
