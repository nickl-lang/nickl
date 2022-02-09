#ifndef HEADER_GUARD_NK_VM_VALUE
#define HEADER_GUARD_NK_VM_VALUE

#include <cassert>

#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"
#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

enum ETypeclassId {
    Type_Array,
    Type_Fn,
    Type_Numeric,
    Type_Ptr,
    Type_Tuple,
    Type_Typeref,
    Type_Void,
};

struct _type_null {
    uint8_t _dummy;
};

struct _type_array {
    type_t elem_type;
    size_t elem_count;
};

using FuncPtr = void (*)(type_t self, value_t ret, value_t args);

//@Refactor Maybe separate function types from actual executable ones?
struct _type_fn {
    type_t ret_t;
    type_t args_t;
    union {
        FuncPtr ptr;
        void *native_fn_info;
    } body;
    void *closure;
    bool is_native;
};

// 0x<index><size>
// Ordered in coercion order, max(lhs, rhs) is a common type
enum ENumericValueType {
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
};

#define NUM_TYPE_SIZE(value_type) (0xf & (value_type))
#define NUM_TYPE_INDEX(value_type) ((0xf0 & (value_type)) >> 4)

struct _type_numeric {
    ENumericValueType value_type;
};

struct _type_ptr {
    type_t target_type;
};

struct TupleElemInfo {
    type_t type;
    size_t offset;
};

using TupleElemInfoArray = Slice<TupleElemInfo>;

struct _type_tuple {
    TupleElemInfoArray elems;
};

struct Type {
    union {
        _type_null null;

        _type_array arr;
        _type_fn fn;
        _type_numeric num;
        _type_ptr ptr;
        _type_tuple tuple;
    } as;
    mutable void *native_handle;
    uint64_t id;
    uint64_t size;
    uint8_t alignment;
    uint8_t typeclass_id;
};

using TypeArray = Slice<type_t const>;

void types_init();
void types_deinit();

type_t type_get_array(type_t elem_type, size_t elem_count);
//@Feature Implement function pointer type
//@Feature Implement callbacks from native code
type_t type_get_fn(type_t ret_t, type_t args_t, size_t decl_id, FuncPtr body_ptr, void *closure);
type_t type_get_fn_native(
    type_t ret_t,
    type_t args_t,
    size_t decl_id,
    void *body_ptr,
    void *closure,
    bool is_variadic);
type_t type_get_numeric(ENumericValueType value_type);
type_t type_get_ptr(type_t target_type);
//@Feature Implement optimized tuple type
type_t type_get_tuple(TypeArray types);
type_t type_get_typeref();
type_t type_get_void();

string type_name(type_t type);

string val_inspect(value_t val);

size_t val_tuple_size(value_t self);
value_t val_tuple_at(value_t self, size_t i);

size_t type_tuple_size(type_t tuple_t);
type_t type_tuple_typeAt(type_t tuple_t, size_t i);
size_t type_tuple_offsetAt(type_t tuple_t, size_t i);

size_t val_array_size(value_t self);
value_t val_array_at(value_t self, size_t i);

size_t type_array_size(type_t array_t);
type_t type_array_elemType(type_t array_t);

void val_fn_invoke(type_t self, value_t ret, value_t args);

inline value_t val_undefined() {
    return value_t{nullptr, nullptr};
}

inline void *val_data(value_t val) {
    return val.data;
}

inline type_t val_typeof(value_t val) {
    return val.type;
}

inline typeid_t val_typeid(value_t val) {
    return val.type->id;
}

inline typeid_t val_typeclassid(value_t val) {
    return val.type->typeclass_id;
}

inline size_t val_sizeof(value_t val) {
    return val.type->size;
}

inline size_t val_alignof(value_t val) {
    return val.type->alignment;
}

inline value_t val_reinterpret_cast(type_t type, value_t val) {
    return value_t{val.data, type};
}

#define val_as(type, val) (*(type *)(val).data)

template <class F>
void val_numeric_visit(value_t val, F &&f) {
    switch (val_typeof(val)->as.num.value_type) {
    case Int8:
        f(val_as(int8_t, val));
        break;
    case Uint8:
        f(val_as(uint8_t, val));
        break;
    case Int16:
        f(val_as(int16_t, val));
        break;
    case Uint16:
        f(val_as(uint16_t, val));
        break;
    case Int32:
        f(val_as(int32_t, val));
        break;
    case Uint32:
        f(val_as(uint32_t, val));
        break;
    case Int64:
        f(val_as(int64_t, val));
        break;
    case Uint64:
        f(val_as(uint64_t, val));
        break;
    case Float32:
        f(val_as(float, val));
        break;
    case Float64:
        f(val_as(double, val));
        break;
    default:
        assert(!"unreachable");
        break;
    }
}

template <class F>
void val_numeric_visit_int(value_t val, F &&f) {
    switch (val_typeof(val)->as.num.value_type) {
    case Int8:
        f(val_as(int8_t, val));
        break;
    case Uint8:
        f(val_as(uint8_t, val));
        break;
    case Int16:
        f(val_as(int16_t, val));
        break;
    case Uint16:
        f(val_as(uint16_t, val));
        break;
    case Int32:
        f(val_as(int32_t, val));
        break;
    case Uint32:
        f(val_as(uint32_t, val));
        break;
    case Int64:
        f(val_as(int64_t, val));
        break;
    case Uint64:
        f(val_as(uint64_t, val));
        break;
    default:
        assert(!"unreachable");
        break;
    }
}
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_VALUE
