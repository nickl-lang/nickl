#ifndef HEADER_GUARD_NK_VM_VALUE
#define HEADER_GUARD_NK_VM_VALUE

#include <cstddef>
#include <cstdint>

#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"

namespace nk {
namespace vm {

struct Type;

using type_t = Type const *;

using typeid_t = size_t;
using typeclassid_t = uint8_t;

enum ETypeclassId {
    Type_Array,
    Type_Fn,
    Type_Numeric,
    Type_Ptr,
    Type_Tuple,
    Type_Typeref,
    Type_Void,
};

struct value_t {
    void *data;
    type_t type;
};

struct _type_null {
    uint8_t _dummy;
};

struct _type_array {
    type_t elem_type;
    size_t elem_count;
};

using FuncPtr = void (*)(type_t self, value_t ret, value_t args);

struct _type_fn {
    type_t ret_type;
    type_t param_types_tuple;
    union {
        void *native_ptr;
        FuncPtr ptr;
    } body;
    void *closure;
};

// 0x<index><size>
// Ordered in coercion order, max(lhs, rhs) is a common type
enum ENumericValueType {
    Int0 = 0x00,
    Uint0 = 0x10,
    Int1 = 0x21,
    Uint1 = 0x31,
    Int8 = 0x41,
    Uint8 = 0x51,
    Int16 = 0x62,
    Uint16 = 0x72,
    Int32 = 0x84,
    Uint32 = 0x94,
    Int64 = 0xa8,
    Uint64 = 0xb8,
    Float32 = 0xc4,
    Float64 = 0xd8,
};

#define NUM_TYPE_SIZE_MASK 0xf

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

struct TupleElemInfoArray {
    size_t size;
    TupleElemInfo *data;
};

struct _type_tuple {
    TupleElemInfoArray types;
};

struct Type {
    union {
        _type_null null_t;

        _type_array arr_t;
        _type_fn fn_t;
        _type_numeric num_t;
        _type_ptr ptr_t;
        _type_tuple tuple_t;
    } as;
    uint64_t id;
    uint64_t size;
    uint8_t alignment;
    uint8_t typeclass_id;
};

struct TypeArray {
    size_t size;
    type_t const *data;
};

void types_init();
void types_deinit();

type_t type_get_array(type_t elem_type, size_t elem_count);
type_t type_get_fn(type_t ret_t, type_t params_t, size_t decl_id, void *body_ptr, void *closure);
type_t type_get_numeric(ENumericValueType value_type);
type_t type_get_ptr(type_t target_type);
type_t type_get_tuple(Allocator *tmp_allocator, TypeArray types);
type_t type_get_typeref();
type_t type_get_void();

string type_name(Allocator *allocator, type_t type);

value_t val_undefined();

void *val_data(value_t val);
type_t val_typeof(value_t val);
size_t val_sizeof(value_t val);
size_t val_alignof(value_t val);

value_t val_reinterpret_cast(type_t type, value_t val);

bool val_isTrue(value_t val);

#define val_as(type, val) (*(type *)(val).data)

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_VALUE
