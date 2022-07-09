#ifndef HEADER_GUARD_NK_VM_VALUE
#define HEADER_GUARD_NK_VM_VALUE

#include <cassert>

#include "nk/mem/stack_allocator.hpp"
#include "nk/str/string.hpp"
#include "nk/str/string_builder.hpp"
#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

enum ETypeclassId {
    Type_Array,
    Type_Fn,
    Type_Numeric,
    Type_Ptr,
    Type_Tuple,
    Type_Void,

    Typeclass_Count,
};

struct _type_array {
    type_t elem_type;
    size_t elem_count;
};

//@Refactor Maybe separate function types from actual executable ones?
struct _type_fn {
    type_t ret_t;
    type_t args_t;
    FuncPtr body;
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

struct TupleLayout {
    TupleElemInfoArray info_ar;
    size_t size;
    size_t align;
};

struct Type {
    union {
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

struct FpBase {
    typeclassid_t id;
};

struct TypeQueryRes {
    Type *type;
    bool inserted;
};

TupleLayout calcTupleLayout(Slice<type_t const> types, Allocator &allocator, size_t stride = 1);

struct types {
    static void init();
    static void deinit();

    static type_t get_array(type_t elem_type, size_t elem_count);

    //@Feature Implement function pointer type
    //@Feature Implement callbacks from native code
    static type_t get_fn(
        type_t ret_t,
        type_t args_t,
        size_t decl_id,
        FuncPtr body_ptr,
        void *closure);

    static type_t get_fn_native(
        type_t ret_t,
        type_t args_t,
        size_t decl_id,
        void *body_ptr,
        bool is_variadic);

    static type_t get_numeric(ENumericValueType value_type);

    static type_t get_ptr(type_t target_type);

    //@Feature Implement optimized tuple type
    static type_t get_tuple(Slice<type_t const> types, size_t stride = 1);

    static type_t get_void();

    static StringBuilder &inspect(type_t type, StringBuilder &sb);

    static size_t tuple_size(type_t tuple_t);
    static type_t tuple_typeAt(type_t tuple_t, size_t i);
    static size_t tuple_offsetAt(type_t tuple_t, size_t i);

    static size_t array_size(type_t array_t);
    static type_t array_elemType(type_t array_t);

protected:
    static TypeQueryRes getType(Slice<uint8_t const> fp, size_t type_size = sizeof(Type));

    static StackAllocator s_typearena;
};

StringBuilder &val_inspect(value_t val, StringBuilder &sb);

size_t val_tuple_size(value_t self);
value_t val_tuple_at(value_t self, size_t i);

size_t val_array_size(value_t self);
value_t val_array_at(value_t self, size_t i);

value_t val_ptr_deref(value_t self);

void val_fn_invoke(type_t self, value_t ret, value_t args);

inline value_t val_undefined() {
    return value_t{};
}

inline void *val_data(value_t val) {
    return val.data;
}

inline type_t val_typeof(value_t val) {
    return val.type;
}

inline typeid_t val_typeid(value_t val) {
    return val_typeof(val)->id;
}

inline typeid_t val_typeclassid(value_t val) {
    return val_typeof(val)->typeclass_id;
}

inline size_t val_sizeof(value_t val) {
    return val_typeof(val)->size;
}

inline size_t val_alignof(value_t val) {
    return val_typeof(val)->alignment;
}

inline value_t val_reinterpret_cast(type_t type, value_t val) {
    return value_t{val_data(val), type};
}

inline value_t val_copy(value_t val, void *dst) {
    Slice<uint8_t> src_slice{(uint8_t *)val_data(val), val_sizeof(val)};
    Slice<uint8_t> dst_slice{(uint8_t *)dst, val_sizeof(val)};
    return value_t{src_slice.copy(dst_slice), val_typeof(val)};
}

inline value_t val_copy(value_t val, Allocator &allocator) {
    return val_copy(val, allocator.alloc_aligned(val_sizeof(val), val_alignof(val)));
}

#define val_as(TYPE, VAL) (*(TYPE *)val_data(VAL))

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
